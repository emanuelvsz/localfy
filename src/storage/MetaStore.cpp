#include "storage/MetaStore.h"

#include <ArduinoJson.h>
#include <SD.h>

#include "config/Config.h"

namespace {
std::vector<Meta> s_meta;

Meta *findMutable(const String &file) {
  for (auto &m : s_meta)
    if (m.file == file) return &m;
  return nullptr;
}

// Grava num arquivo temporario e so entao substitui: se faltar energia no
// meio da escrita, o meta.json antigo continua intacto.
bool save() {
  JsonDocument doc;
  for (auto &m : s_meta) {
    if (m.artist.isEmpty() && m.genre.isEmpty()) continue;
    JsonObject o = doc[m.file].to<JsonObject>();
    o["a"] = m.artist;
    o["g"] = m.genre;
  }
  SD.remove(cfg::path::META_TMP);
  File f = SD.open(cfg::path::META_TMP, FILE_WRITE);
  if (!f) return false;
  bool ok = serializeJson(doc, f) > 0;
  f.flush();
  f.close();
  if (!ok) {
    SD.remove(cfg::path::META_TMP);
    return false;
  }
  SD.remove(cfg::path::META);
  return SD.rename(cfg::path::META_TMP, cfg::path::META);
}
}  // namespace

void meta::load() {
  s_meta.clear();
  if (!SD.exists(cfg::path::META)) return;
  File f = SD.open(cfg::path::META, FILE_READ);
  if (!f) return;
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) {
    Serial.printf("[META] json invalido: %s\n", err.c_str());
    return;
  }
  for (JsonPair kv : doc.as<JsonObject>()) {
    Meta m;
    m.file = kv.key().c_str();
    m.artist = kv.value()["a"] | "";
    m.genre = kv.value()["g"] | "";
    s_meta.push_back(m);
  }
  Serial.printf("[META] %u entradas\n", (unsigned)s_meta.size());
}

const Meta *meta::find(const String &file) { return findMutable(file); }

void meta::set(const String &file, const String &artist, const String &genre) {
  Meta *m = findMutable(file);
  if (!m) {
    s_meta.push_back({file, artist, genre});
  } else {
    m->artist = artist;
    m->genre = genre;
  }
  save();
}

const std::vector<Meta> &meta::all() { return s_meta; }
