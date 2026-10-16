#include "audio/AudioPipeline.h"

#include <SD.h>
#include <freertos/stream_buffer.h>

#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"
#include "audio/Duration.h"
#include "audio/Player.h"
#include "bluetooth/Bluetooth.h"
#include "config/Config.h"
#include "core/Lock.h"

namespace {
constexpr size_t BYTES_PER_FRAME = 4;   // estereo, 16 bits por canal

// Buffer PCM entre as duas tasks. StreamBuffer do FreeRTOS e seguro para um
// produtor e um consumidor em tasks distintas, sem mutex adicional.
StreamBufferHandle_t s_buf = nullptr;

// Formato da faixa aberta. O Bluetooth toca sempre 44,1 kHz estereo; faixas
// em outra taxa (48 kHz e comum) ou mono passam pelo conversor abaixo. Sem
// ele, uma faixa de 48 kHz tocava ~8% mais lenta e grave ("slowed").
uint32_t s_srcRate = cfg::audio::SAMPLE_RATE;
uint8_t s_srcChannels = 2;

// Conversor de taxa por interpolacao linear. s_phase e a posicao (Q16) entre
// o quadro anterior e o atual; cada quadro de saida avanca s_step.
uint32_t s_step = 65536;
uint32_t s_phase = 0;
int16_t s_prev[2] = {0, 0};
bool s_havePrev = false;
uint8_t s_carry[4];        // pedaco de quadro que sobrou do write anterior
size_t s_carryLen = 0;

void resetConverter() {
  s_step = (uint32_t)(((uint64_t)s_srcRate << 16) / cfg::audio::SAMPLE_RATE);
  s_phase = 0;
  s_havePrev = false;
  s_carryLen = 0;
}

// Timeout curto: se o fone parar de consumir, nao prende a task.
size_t sendPcm(const void *data, size_t len) {
  return xStreamBufferSend(s_buf, data, len, pdMS_TO_TICKS(100));
}

// Destino do decoder: grava as amostras PCM no buffer circular.
class PcmSink : public Print {
 public:
  size_t write(const uint8_t *data, size_t len) override {
    if (!s_buf || len == 0) return 0;
    if (s_srcRate == cfg::audio::SAMPLE_RATE && s_srcChannels == 2) return sendPcm(data, len);
    convert(data, len);
    return len;   // o que nao coube (fone parado) e descartado, como um engasgo
  }
  size_t write(uint8_t b) override { return write(&b, 1); }
  int availableForWrite() override {
    return s_buf ? (int)xStreamBufferSpacesAvailable(s_buf) : 0;
  }

 private:
  static constexpr size_t OUT_FRAMES = 128;
  int16_t out_[OUT_FRAMES * 2];
  size_t outN_ = 0;

  void flush() {
    if (outN_) sendPcm(out_, outN_ * 4);
    outN_ = 0;
  }

  void emit(int16_t l, int16_t r) {
    out_[outN_ * 2] = l;
    out_[outN_ * 2 + 1] = r;
    if (++outN_ == OUT_FRAMES) flush();
  }

  void frame(int16_t l, int16_t r) {
    if (!s_havePrev) {
      s_prev[0] = l;
      s_prev[1] = r;
      s_havePrev = true;
      return;
    }
    while (s_phase < 65536) {
      int32_t f = (int32_t)s_phase;
      emit((int16_t)(s_prev[0] + (((int32_t)(l - s_prev[0]) * f) >> 16)),
           (int16_t)(s_prev[1] + (((int32_t)(r - s_prev[1]) * f) >> 16)));
      s_phase += s_step;
    }
    s_phase -= 65536;
    s_prev[0] = l;
    s_prev[1] = r;
  }

  void convert(const uint8_t *data, size_t len) {
    const size_t fb = s_srcChannels * 2;   // bytes por quadro de entrada
    size_t i = 0;
    while (s_carryLen && i < len) {        // completa o quadro partido
      s_carry[s_carryLen++] = data[i++];
      if (s_carryLen == fb) {
        consume(s_carry);
        s_carryLen = 0;
      }
    }
    for (; i + fb <= len; i += fb) consume(data + i);
    while (i < len) s_carry[s_carryLen++] = data[i++];
    flush();
  }

  void consume(const uint8_t *p) {
    int16_t l, r;
    memcpy(&l, p, 2);
    if (s_srcChannels == 2) memcpy(&r, p + 2, 2);
    else r = l;   // mono: o mesmo som nos dois lados
    frame(l, r);
  }
};

PcmSink s_pcm;
EncodedAudioStream s_decoder(&s_pcm, new MP3DecoderHelix());
StreamCopy s_copier;
File s_file;
uint32_t s_audioStart = 0;   // posicoes do player sao relativas a este byte

// O decoder e alocado uma unica vez e reaproveitado entre faixas: o begin()
// da libhelix libera e realoca os buffers, e com o Bluetooth ativo a memoria
// fica fragmentada e a realocacao falhava ao trocar de faixa.
bool s_decoderReady = false;

// Relogio da reproducao: quadros efetivamente entregues ao fone. Conta so
// amostras reais, nunca o silencio de preenchimento, entao o tempo decorrido
// e exato mesmo em MP3 de taxa variavel.
volatile uint64_t s_frames = 0;

// Ganho em Q15 (32768 = 100%). Curva quadratica: o ouvido percebe volume de
// forma logaritmica, e assim a metade do knob soa como "metade".
volatile int32_t s_gain = 32768;

volatile uint32_t s_gaps = 0;
volatile uint32_t s_maxStepUs = 0;
volatile uint32_t s_steps = 0;
TaskHandle_t s_task = nullptr;

// Enche o buffer em rajada e so dorme quando ele esta cheio.
void decoderTask(void *) {
  for (;;) {
    bool worked = false;
    while (player::isPlaying() && bluetooth::isConnected() && s_buf &&
           xStreamBufferSpacesAvailable(s_buf) > cfg::audio::PCM_HEADROOM) {
      if (!lock::tryTake(20)) break;
      bool ok = player::isPlaying() && s_file && s_file.available();
      if (ok) {
        uint32_t t0 = micros();
        s_copier.copy();
        uint32_t dt = micros() - t0;
        if (dt > s_maxStepUs) s_maxStepUs = dt;
        s_steps++;
        worked = true;
      }
      lock::give();
      if (!ok) break;
    }
    vTaskDelay(pdMS_TO_TICKS(worked ? 1 : 3));
  }
}
}  // namespace

bool pipeline::reserveDecoder() {
  if (!s_decoderReady) s_decoderReady = s_decoder.begin();
  return s_decoderReady;
}

void pipeline::startDecoderTask() {
  xTaskCreatePinnedToCore(decoderTask, "decoder", cfg::audio::DECODER_STACK, nullptr,
                          cfg::audio::DECODER_PRIORITY, &s_task, cfg::audio::DECODER_CORE);
}

bool pipeline::createBuffer() {
  if (!s_buf) s_buf = xStreamBufferCreate(cfg::audio::PCM_BUFFER_BYTES, 1);
  return s_buf != nullptr;
}

pipeline::OpenResult pipeline::open(const String &path, bool isWav) {
  OpenResult r;
  s_file = SD.open(path, FILE_READ);
  if (!s_file) {
    Serial.printf("[PLAY] nao abriu %s\n", path.c_str());
    return r;
  }
  duration::Info info = isWav ? duration::ofWav(s_file) : duration::ofMp3(s_file);
  s_audioStart = info.audioStart;
  s_srcRate = info.sampleRate ? info.sampleRate : cfg::audio::SAMPLE_RATE;
  s_srcChannels = info.channels;
  resetConverter();
  if (s_srcRate != cfg::audio::SAMPLE_RATE || s_srcChannels != 2)
    Serial.printf("[PLAY] convertendo %lu Hz %s -> 44100 Hz estereo\n", (unsigned long)s_srcRate,
                  s_srcChannels == 1 ? "mono" : "estereo");
  r.sizeBytes = s_file.size() > s_audioStart ? s_file.size() - s_audioStart : 0;
  r.durationSecs = info.secs;
  r.durationExact = info.exact;
  // Comeca direto no audio: sem o cabecalho WAV nem a etiqueta ID3 (capa).
  s_file.seek(s_audioStart);
  if (s_audioStart > 0)
    Serial.printf("[PLAY] audio a partir do byte %lu\n", (unsigned long)s_audioStart);

  if (isWav) {
    // WAV ja e PCM: vai direto ao buffer.
    s_copier.begin(s_pcm, s_file);
  } else {
    // Sem novo begin(): o decoder ressincroniza sozinho no proximo quadro.
    if (!reserveDecoder()) {
      Serial.println("[PLAY] decoder MP3 sem memoria");
      s_file.close();
      return r;
    }
    s_copier.begin(s_decoder, s_file);
  }
  r.ok = true;
  return r;
}

void pipeline::close() {
  s_copier.end();
  if (s_buf) xStreamBufferReset(s_buf);   // descarta o resto da faixa anterior
  s_frames = 0;
  if (s_file) s_file.close();
}

bool pipeline::finished() {
  return !s_file.available() && (!s_buf || xStreamBufferIsEmpty(s_buf));
}

void pipeline::seek(uint32_t bytePos, uint32_t secs) {
  if (!s_file) return;
  s_file.seek(s_audioStart + bytePos);
  resetConverter();
  if (s_buf) xStreamBufferReset(s_buf);
  s_frames = (uint64_t)secs * cfg::audio::SAMPLE_RATE;
  // Pular no meio de um quadro MP3 gera um estalo curto ate o decoder achar o
  // proximo cabecalho; reiniciar o decoder exigiria realocar memoria.
}

uint32_t pipeline::filePosition() {
  if (!s_file) return 0;
  uint32_t pos = s_file.position();
  return pos > s_audioStart ? pos - s_audioStart : 0;
}

uint64_t pipeline::framesPlayed() { return s_frames; }

void pipeline::setVolume(uint8_t percent) {
  if (percent > 100) percent = 100;
  s_gain = (int32_t)percent * percent * 32768 / 10000;
}

void pipeline::fill(uint8_t *dst, size_t bytes) {
  // A pilha Bluetooth as vezes pede 0 quadros sem buffer de destino; repassar
  // isso ao StreamBuffer derruba o ESP32 (assert em xStreamBufferReceive).
  if (!dst || bytes == 0) return;
  size_t got = s_buf ? xStreamBufferReceive(s_buf, dst, bytes, 0) : 0;
  s_frames += got / BYTES_PER_FRAME;
  int32_t gain = s_gain;
  if (gain < 32768) {
    int16_t *s = reinterpret_cast<int16_t *>(dst);
    for (size_t i = 0; i < got / 2; i++) s[i] = (int16_t)((s[i] * gain) >> 15);
  }
  if (got < bytes) {
    memset(dst + got, 0, bytes - got);
    if (player::isPlaying()) s_gaps++;   // buffer vazio durante a musica = engasgo
  }
}

pipeline::Stats pipeline::takeStats() {
  Stats s;
  s.gaps = s_gaps;
  s.maxStepMs = s_maxStepUs / 1000;
  s.steps = s_steps;
  s.stackFree = s_task ? uxTaskGetStackHighWaterMark(s_task) : 0;
  s_gaps = 0;
  s_maxStepUs = 0;
  s_steps = 0;
  return s;
}
