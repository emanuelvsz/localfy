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

// Destino do decoder: grava as amostras PCM no buffer circular.
class PcmSink : public Print {
 public:
  size_t write(const uint8_t *data, size_t len) override {
    if (!s_buf || len == 0) return 0;
    // Timeout curto: se o fone parar de consumir, nao prende a task.
    return xStreamBufferSend(s_buf, data, len, pdMS_TO_TICKS(100));
  }
  size_t write(uint8_t b) override { return write(&b, 1); }
  int availableForWrite() override {
    return s_buf ? (int)xStreamBufferSpacesAvailable(s_buf) : 0;
  }
};

PcmSink s_pcm;
EncodedAudioStream s_decoder(&s_pcm, new MP3DecoderHelix());
StreamCopy s_copier;
File s_file;

// O decoder e alocado uma unica vez e reaproveitado entre faixas: o begin()
// da libhelix libera e realoca os buffers, e com o Bluetooth ativo a memoria
// fica fragmentada e a realocacao falhava ao trocar de faixa.
bool s_decoderReady = false;

// Relogio da reproducao: quadros efetivamente entregues ao fone. Conta so
// amostras reais, nunca o silencio de preenchimento, entao o tempo decorrido
// e exato mesmo em MP3 de taxa variavel.
volatile uint64_t s_frames = 0;

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
  r.sizeBytes = s_file.size();
  r.durationSecs = isWav ? duration::ofWav(s_file) : duration::ofMp3(s_file);

  if (isWav) {
    // WAV ja e PCM: vai direto ao buffer, pulando o cabecalho.
    s_file.seek(44);
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
  s_file.seek(bytePos);
  if (s_buf) xStreamBufferReset(s_buf);
  s_frames = (uint64_t)secs * cfg::audio::SAMPLE_RATE;
  // Pular no meio de um quadro MP3 gera um estalo curto ate o decoder achar o
  // proximo cabecalho; reiniciar o decoder exigiria realocar memoria.
}

uint32_t pipeline::filePosition() { return s_file ? s_file.position() : 0; }

uint64_t pipeline::framesPlayed() { return s_frames; }

void pipeline::fill(uint8_t *dst, size_t bytes) {
  size_t got = s_buf ? xStreamBufferReceive(s_buf, dst, bytes, 0) : 0;
  s_frames += got / BYTES_PER_FRAME;
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
