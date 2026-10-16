#include "audio/Player.h"

#include <Arduino.h>
#include <esp_random.h>

#include <vector>

#include "audio/AudioPipeline.h"
#include "bluetooth/Bluetooth.h"
#include "config/Config.h"
#include "core/Lock.h"
#include "storage/History.h"
#include "storage/Library.h"
#include "storage/Settings.h"

namespace {
volatile bool s_playing = false;
int s_index = -1;
String s_currentName;   // para reencontrar a faixa quando a biblioteca e relida
uint32_t s_durationSecs = 0;
uint32_t s_sizeBytes = 0;
bool s_durationExact = false;   // veio do cabecalho: nao refinar
bool s_isWav = false;
uint8_t s_volume = cfg::ui::DEFAULT_VOLUME;

// Fila: ordem das faixas da pasta escolhida (embaralhada no aleatorio) e a
// posicao da faixa atual nela. Refeita quando a pasta, o aleatorio ou a
// biblioteca mudam.
int s_filter = library::ALL;
bool s_shuffle = false;
std::vector<uint16_t> s_order;
int s_pos = -1;
bool s_orderDirty = true;

// Monta a ordem. 'keep' (faixa atual) vira a posicao atual: no aleatorio ela
// vai para o inicio, para a proxima ja ser sorteada.
void rebuildOrder(int keep) {
  s_order.clear();
  for (size_t i = 0; i < library::count(); i++)
    if (library::inFilter(i, s_filter)) s_order.push_back((uint16_t)i);
  if (s_shuffle) {
    for (int i = (int)s_order.size() - 1; i > 0; i--) {
      int j = (int)(esp_random() % (uint32_t)(i + 1));
      std::swap(s_order[i], s_order[j]);
    }
  }
  s_pos = -1;
  for (size_t i = 0; i < s_order.size(); i++) {
    if (s_order[i] == keep) {
      if (s_shuffle) std::swap(s_order[0], s_order[i]), s_pos = 0;
      else s_pos = (int)i;
      break;
    }
  }
  s_orderDirty = false;
}

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
    history::append(library::nameOf(s_index), secs, s_durationSecs);
    s_scrobbled = true;
  }
}

// A duracao real so e conhecida no fim. Ate la e estimada pela taxa de bits
// MEDIA observada (bytes lidos / tempo tocado), que converge rapido mesmo em
// VBR, ao contrario da taxa do primeiro quadro.
void refineDuration() {
  if (s_isWav || s_durationExact || s_sizeBytes == 0) return;
  uint64_t frames = pipeline::framesPlayed();
  if (frames < (uint64_t)cfg::audio::SAMPLE_RATE * 3) return;   // espera estabilizar
  uint32_t pos = pipeline::filePosition();
  if (pos == 0) return;
  s_durationSecs = (uint32_t)(((uint64_t)s_sizeBytes * frames) /
                              ((uint64_t)pos * cfg::audio::SAMPLE_RATE));
}

bool openTrack(int index);

// Proxima/anterior DENTRO da fila. No aleatorio, ao dar a volta a fila e
// sorteada de novo (nao repete a mesma sequencia).
void step(int delta) {
  if (s_orderDirty) rebuildOrder(s_index);
  int n = (int)s_order.size();
  if (n == 0) return;
  if (s_pos < 0) {
    s_pos = delta < 0 ? n - 1 : 0;
  } else {
    int next = s_pos + delta;
    if (s_shuffle && next >= n) {
      rebuildOrder(-1);
      next = 0;
    }
    s_pos = ((next % n) + n) % n;
  }
  openTrack(s_order[s_pos]);
}
}  // namespace

// Escolha direta de uma faixa (lista do aparelho ou interface): se ela estiver
// fora da pasta da fila, a fila passa a ser "Todas".
bool player::play(int index) {
  if (index < 0 || index >= (int)library::count()) return false;
  if (!library::inFilter(index, s_filter)) {
    s_filter = library::ALL;
    s_orderDirty = true;
  }
  if (s_orderDirty) rebuildOrder(index);
  for (size_t i = 0; i < s_order.size(); i++)
    if (s_order[i] == index) s_pos = (int)i;
  return openTrack(index);
}

namespace {
bool openTrack(int index) {
  flushScrobble();
  pipeline::close();

  String name = library::nameOf(index);
  s_isWav = library::extension(name) == "wav";
  pipeline::OpenResult r = pipeline::open(library::pathOf(name), s_isWav);
  if (!r.ok) {
    s_playing = false;
    return false;
  }

  s_durationSecs = r.durationSecs;
  s_sizeBytes = r.sizeBytes;
  s_durationExact = r.durationExact;
  s_index = index;
  s_currentName = name;
  s_playing = true;
  s_listenedMs = 0;
  s_scrobbled = false;
  s_lastTickMs = millis();
  Serial.printf("[PLAY] %s (%lus)\n", name.c_str(), (unsigned long)s_durationSecs);
  return true;
}
}  // namespace

void player::setQueue(int filter, bool shuffle) {
  s_filter = filter;
  s_shuffle = shuffle;
  rebuildOrder(s_index);
  String name = filter == library::ALL    ? String("")
                : filter == library::ROOT ? String("/")
                                          : library::filterLabel(filter);
  settings::setQueue(name, shuffle);
  Serial.printf("[FILA] %s%s (%u faixas)\n", library::filterLabel(filter).c_str(),
                shuffle ? ", aleatorio" : "", (unsigned)s_order.size());
}

int player::queueFilter() { return s_filter; }
bool player::shuffle() { return s_shuffle; }

void player::loadQueue() {
  String f = settings::queueFolder();
  s_filter = f.isEmpty() ? library::ALL : f == "/" ? library::ROOT : library::findFolder(f);
  if (s_filter == -1 && !f.isEmpty()) s_filter = library::ALL;   // pasta sumiu
  s_shuffle = settings::queueShuffle();
  s_orderDirty = true;
  Serial.printf("[FILA] %s%s\n", library::filterLabel(s_filter).c_str(),
                s_shuffle ? ", aleatorio" : "");
}

void player::onLibraryChanged() {
  // Os indices mudaram: reencontra a faixa atual pelo nome.
  if (s_index >= 0) {
    s_index = library::find(s_currentName);
    if (s_index < 0) stop();   // a faixa tocando foi apagada ou movida
  }
  s_orderDirty = true;
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
  pipeline::seek((uint32_t)target, secs);
}

void player::setVolume(uint8_t percent) {
  if (percent > 100) percent = 100;
  s_volume = percent;
  pipeline::setVolume(percent);
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
