#include "storage/Library.h"

#include <SD.h>

#include <algorithm>

#include "config/Config.h"

namespace {
std::vector<char> s_pool;          // "Joji\0LOVE YOU LESS.mp3\0..."
std::vector<uint16_t> s_folders;   // posicao do nome de cada pasta no bloco
std::vector<Song> s_songs;

const char *at(uint16_t off) { return s_pool.data() + off; }

uint16_t store(const String &text) {
  uint16_t off = (uint16_t)s_pool.size();
  s_pool.insert(s_pool.end(), text.c_str(), text.c_str() + text.length() + 1);
  return off;
}

String lastSegment(const String &path) {
  int slash = path.lastIndexOf('/');
  return slash >= 0 ? path.substring(slash + 1) : path;
}

// Arquivos de audio de uma pasta (sem descer mais niveis).
void scanDir(File &dir, int16_t folder) {
  File f;
  while ((f = dir.openNextFile()) && s_songs.size() < cfg::sd::MAX_SONGS) {
    if (!f.isDirectory()) {
      String n = lastSegment(f.name());
      // O bloco usa posicoes de 16 bits: para antes de estourar 64 KB.
      if (library::isAudio(n) && !n.startsWith(".") && s_pool.size() + n.length() < 65000)
        s_songs.push_back({store(n), folder, (uint32_t)f.size()});
    }
    f.close();
  }
}
}  // namespace

void library::scan() {
  s_songs.clear();
  s_folders.clear();
  s_pool.clear();
  File dir = SD.open(cfg::path::MUSIC_DIR);
  if (!dir || !dir.isDirectory()) {
    Serial.printf("[SD] %s inexistente, criando\n", cfg::path::MUSIC_DIR);
    SD.mkdir(cfg::path::MUSIC_DIR);
    return;
  }

  // 1a passada: pastas de artista, em ordem alfabetica.
  std::vector<String> names;
  File f;
  while ((f = dir.openNextFile())) {
    if (f.isDirectory()) {
      String n = lastSegment(f.name());
      if (isSafeFolder(n)) names.push_back(n);
    }
    f.close();
  }
  std::sort(names.begin(), names.end());
  for (auto &n : names) s_folders.push_back(store(n));

  // 2a passada: musicas da raiz e de cada pasta.
  dir.rewindDirectory();
  scanDir(dir, -1);
  dir.close();
  for (size_t i = 0; i < names.size(); i++) {
    File sub = SD.open(String(cfg::path::MUSIC_DIR) + "/" + names[i]);
    if (sub && sub.isDirectory()) scanDir(sub, (int16_t)i);
    sub.close();
  }
  names.clear();
  names.shrink_to_fit();

  // Soltas primeiro, depois pasta a pasta; dentro de cada uma, por nome.
  std::sort(s_songs.begin(), s_songs.end(), [](const Song &a, const Song &b) {
    if (a.folder != b.folder) return a.folder < b.folder;
    return strcasecmp(at(a.file), at(b.file)) < 0;
  });
  // Devolve a sobra de capacidade: o que fica e exatamente o necessario.
  s_pool.shrink_to_fit();
  s_songs.shrink_to_fit();
  s_folders.shrink_to_fit();
  Serial.printf("[SD] %u musicas em %u pastas (%u bytes de nomes)\n", (unsigned)s_songs.size(),
                (unsigned)s_folders.size(), (unsigned)s_pool.size());
}

const std::vector<Song> &library::songs() { return s_songs; }
size_t library::count() { return s_songs.size(); }

String library::nameOf(size_t i) {
  if (i >= s_songs.size()) return "";
  const Song &s = s_songs[i];
  if (s.folder < 0) return at(s.file);
  String n = at(s_folders[s.folder]);
  n += '/';
  n += at(s.file);
  return n;
}

const char *library::fileOf(size_t i) { return i < s_songs.size() ? at(s_songs[i].file) : ""; }

size_t library::folderCount() { return s_folders.size(); }

const char *library::folderName(int folder) {
  return folder >= 0 && folder < (int)s_folders.size() ? at(s_folders[folder]) : "";
}

int library::findFolder(const String &name) {
  for (size_t i = 0; i < s_folders.size(); i++)
    if (name == at(s_folders[i])) return (int)i;
  return -1;
}

bool library::hasRootSongs() {
  return !s_songs.empty() && s_songs.front().folder < 0;   // soltas vem primeiro
}

bool library::inFilter(size_t i, int filter) {
  if (i >= s_songs.size()) return false;
  if (filter == ALL) return true;
  if (filter == ROOT) return s_songs[i].folder < 0;
  return s_songs[i].folder == filter;
}

size_t library::countIn(int filter) {
  size_t n = 0;
  for (size_t i = 0; i < s_songs.size(); i++)
    if (inFilter(i, filter)) n++;
  return n;
}

String library::filterLabel(int filter) {
  if (filter == ALL) return "Todas";
  if (filter == ROOT) return "Sem pasta";
  return filter >= 0 && filter < (int)s_folders.size() ? String(at(s_folders[filter])) : String("?");
}

// Compara direto com o bloco, sem montar uma String por musica.
int library::find(const String &name) {
  int slash = name.indexOf('/');
  int folder = -1;
  String file = name;
  if (slash >= 0) {
    folder = findFolder(name.substring(0, slash));
    if (folder < 0) return -1;
    file = name.substring(slash + 1);
  }
  for (size_t i = 0; i < s_songs.size(); i++)
    if (s_songs[i].folder == folder && file == at(s_songs[i].file)) return (int)i;
  return -1;
}

String library::pathOf(const String &name) {
  return String(cfg::path::MUSIC_DIR) + "/" + name;
}

String library::fileName(const String &name) { return lastSegment(name); }

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

bool library::isSafeFolder(const String &folder) {
  if (folder.isEmpty() || folder.length() > 48) return false;
  if (folder.startsWith(".") || folder.indexOf("..") >= 0) return false;
  return folder.indexOf('/') < 0 && folder.indexOf('\\') < 0;
}

bool library::isSafeName(const String &name) {
  if (name.length() == 0 || name.length() > cfg::sd::MAX_NAME_LEN) return false;
  if (name.indexOf('\\') >= 0 || name.indexOf("..") >= 0 || name.startsWith("/")) return false;
  int slash = name.indexOf('/');
  if (slash >= 0) {
    if (name.indexOf('/', slash + 1) >= 0) return false;   // um nivel so
    if (!isSafeFolder(name.substring(0, slash))) return false;
  }
  String file = lastSegment(name);
  return !file.isEmpty() && !file.startsWith(".") && isAudio(file);
}
