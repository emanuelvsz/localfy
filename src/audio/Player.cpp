#include "audio/Player.h"

#include <Arduino.h>

#include "audio/AudioPipeline.h"
#include "bluetooth/Bluetooth.h"
#include "config/Config.h"
#include "core/Lock.h"
#include "storage/History.h"
#include "storage/Library.h"

namespace {
volatile bool s_playing = false;
int s_index = -1;
uint32_t s_durationSecs = 0;
uint32_t s_sizeBytes = 0;
bool s_isWav = false;
uint8_t s_volume = cfg::ui::DEFAULT_VOLUME;

// Contabilidade do historico da faixa atual.
uint32_t s_listenedMs = 0;
uint32_t s_lastTickMs = 0;
bool s_scrobbled = false;

// Grava a faixa atual no historico, se ela foi ouvida o bastante.
void flushScrobble() {
  if (s_index < 0 || s_scrobbled) return;
  uint32_t secs = s_listenedMs / 1000;
  bool enough = secs >= cfg::scrobble::MIN_SECS ||
                (s_durationSecs > 0 && secs >= s_durationSecs * cfg::scrobble::MIN_RATIO);
  if (enough) {
    history::append(library::songs()[s_index].name, secs, s_durationSecs);
    s_scrobbled = true;
  }
}

// A duracao real so e conhecida no fim. Ate la e estimada pela taxa de bits
// MEDIA observada (bytes lidos / tempo tocado), que converge rapido mesmo em
// VBR, ao contrario da taxa do primeiro quadro.
void refineDuration() {
  if (s_isWav || s_sizeBytes == 0) return;
  uint64_t frames = pipeline::framesPlayed();
  if (frames < (uint64_t)cfg::audio::SAMPLE_RATE * 3) return;   // espera estabilizar
  uint32_t pos = pipeline::filePosition();
  if (pos == 0) return;
  s_durationSecs = (uint32_t)(((uint64_t)s_sizeBytes * frames) /
                              ((uint64_t)pos * cfg::audio::SAMPLE_RATE));
}

void step(int delta) {
  int n = (int)library::count();
  if (n == 0) return;
  int from = s_index < 0 ? 0 : s_index;
  player::play(((from + delta) % n + n) % n);
}
}  // namespace

bool player::play(int index) {
  if (index < 0 || index >= (int)library::count()) return false;

  flushScrobble();
  pipeline::close();

  const String &name = library::songs()[index].name;
  s_isWav = library::extension(name) == "wav";
  pipeline::OpenResult r = pipeline::open(library::pathOf(name), s_isWav);
  if (!r.ok) {
    s_playing = false;
    return false;
  }

  s_durationSecs = r.durationSecs;
  s_sizeBytes = r.sizeBytes;
  s_index = index;
  s_playing = true;
  s_listenedMs = 0;
  s_scrobbled = false;
  s_lastTickMs = millis();
  Serial.printf("[PLAY] %s (%lus)\n", name.c_str(), (unsigned long)s_durationSecs);
  return true;
}

void player::stop() {
  flushScrobble();
  pipeline::close();
  s_playing = false;
  s_index = -1;
  s_durationSecs = s_sizeBytes = 0;
  s_listenedMs = 0;
  s_scrobbled = false;
}

void player::pause() { s_playing = false; }

void player::resume() {
  if (s_index < 0) {
    step(0);
  } else {
    s_playing = true;
    s_lastTickMs = millis();
  }
}

void player::togglePause() {
  if (s_playing) pause();
  else resume();
}

void player::next() { step(1); }

void player::previous() {
  if (positionSecs() > 3) seek(0);
  else step(-1);
}

void player::seek(uint32_t secs) {
  if (s_index < 0 || s_durationSecs == 0 || s_sizeBytes == 0) return;
  if (secs > s_durationSecs) secs = s_durationSecs;
  uint64_t target = ((uint64_t)secs * s_sizeBytes) / s_durationSecs;
  if (s_isWav && target < 44) target = 44;
  pipeline::seek((uint32_t)target, secs);
}

void player::setVolume(uint8_t percent) {
  if (percent > 100) percent = 100;
  s_volume = percent;
  bluetooth::applyVolume(percent);
}

uint8_t player::volume() { return s_volume; }

void player::onLinkLost() {
  if (!s_playing) return;
  flushScrobble();
  s_playing = false;
}

void player::tick() {
  if (!s_playing || !bluetooth::isConnected()) {
    s_lastTickMs = millis();
    return;
  }
  Guard g;
  refineDuration();
  uint32_t now = millis();
  s_listenedMs += now - s_lastTickMs;
  s_lastTickMs = now;

  if (pipeline::finished()) {
    // Fim da faixa: arquivo esgotado e buffer ja tocado.
    if (s_durationSecs > 0) s_listenedMs = s_durationSecs * 1000;
    flushScrobble();
    step(1);
  }
}

int player::currentIndex() { return s_index; }
bool player::isPlaying() { return s_playing; }

uint32_t player::positionSecs() {
  if (s_index < 0) return 0;
  return (uint32_t)(pipeline::framesPlayed() / cfg::audio::SAMPLE_RATE);
}

uint32_t player::durationSecs() { return s_durationSecs; }
