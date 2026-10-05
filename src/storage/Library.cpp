#include "storage/Library.h"

#include <SD.h>

#include <algorithm>

#include "config/Config.h"

namespace {
std::vector<Song> s_songs;
}

void library::scan() {
  s_songs.clear();
  File dir = SD.open(cfg::path::MUSIC_DIR);
  if (!dir || !dir.isDirectory()) {
    Serial.printf("[SD] %s inexistente, criando\n", cfg::path::MUSIC_DIR);
    SD.mkdir(cfg::path::MUSIC_DIR);
    return;
  }

  File f;
  while ((f = dir.openNextFile()) && s_songs.size() < cfg::sd::MAX_SONGS) {
    if (!f.isDirectory()) {
      String n = f.name();
      int slash = n.lastIndexOf('/');
      if (slash >= 0) n = n.substring(slash + 1);
      if (isAudio(n) && !n.startsWith(".")) s_songs.push_back({n, (uint32_t)f.size()});
    }
    f.close();
  }
  dir.close();

  std::sort(s_songs.begin(), s_songs.end(),
            [](const Song &a, const Song &b) { return a.name < b.name; });
  Serial.printf("[SD] %u musicas\n", (unsigned)s_songs.size());
}

const std::vector<Song> &library::songs() { return s_songs; }

size_t library::count() { return s_songs.size(); }

int library::find(const String &name) {
  for (size_t i = 0; i < s_songs.size(); i++)
    if (s_songs[i].name == name) return (int)i;
  return -1;
}

String library::pathOf(const String &name) {
  return String(cfg::path::MUSIC_DIR) + "/" + name;
}

String library::extension(const String &name) {
  int dot = name.lastIndexOf('.');
  if (dot < 0) return "";
  String e = name.substring(dot + 1);
  e.toLowerCase();
  return e;
}

bool library::isAudio(const String &name) {
  String e = extension(name);
  return e == "mp3" || e == "wav";
}

bool library::isSafeName(const String &name) {
  if (name.length() == 0 || name.length() > cfg::sd::MAX_NAME_LEN) return false;
  if (name.indexOf('/') >= 0 || name.indexOf('\\') >= 0) return false;
  if (name.indexOf("..") >= 0) return false;
  return isAudio(name);
}
