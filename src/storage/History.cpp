#include "storage/History.h"

#include <SD.h>
#include <time.h>

#include "config/Config.h"
#include "core/TimeUtil.h"
#include "storage/MetaStore.h"

namespace {
// Escapa um campo para CSV (RFC 4180): aspas duplicadas, campo entre aspas.
String csvEscape(const String &in) {
  String out = "\"";
  for (size_t i = 0; i < in.length(); i++) {
    char c = in[i];
    if (c == '"') out += "\"\"";
    else if (c == '\r' || c == '\n') out += ' ';
    else out += c;
  }
  out += "\"";
  return out;
}
}  // namespace

void history::ensureHeader() {
  if (SD.exists(cfg::path::HISTORY)) return;
  File f = SD.open(cfg::path::HISTORY, FILE_WRITE);
  if (!f) return;
  f.println("timestamp,datetime,file,artist,genre,listened_secs,duration_secs,completed");
  f.close();
}

void history::append(const String &file, uint32_t listenedSecs, uint32_t durationSecs) {
  const Meta *m = meta::find(file);
  time_t now = time(nullptr);
  bool synced = timeutil::synced();

  char dt[24] = "";
  if (synced) {
    struct tm tmv;
    localtime_r(&now, &tmv);
    strftime(dt, sizeof(dt), "%Y-%m-%d %H:%M:%S", &tmv);
  }

  File f = SD.open(cfg::path::HISTORY, FILE_APPEND);
  if (!f) {
    Serial.println("[HIST] falha ao abrir");
    return;
  }

  // Sem artista cadastrado, vale o nome da pasta ("Sabrina Carpenter/...").
  String artist = m ? m->artist : String("");
  int slash = file.indexOf('/');
  if (artist.isEmpty() && slash > 0) artist = file.substring(0, slash);

  bool completed = durationSecs > 0 && listenedSecs >= (durationSecs * 9) / 10;
  f.printf("%lu,%s,%s,%s,%s,%lu,%lu,%d\n",
           synced ? (unsigned long)now : 0UL,
           csvEscape(dt).c_str(),
           csvEscape(file).c_str(),
           csvEscape(artist).c_str(),
           csvEscape(m ? m->genre : "").c_str(),
           (unsigned long)listenedSecs,
           (unsigned long)durationSecs,
           completed ? 1 : 0);
  f.flush();
  f.close();
  Serial.printf("[HIST] %s (%lus)\n", file.c_str(), (unsigned long)listenedSecs);
}

bool history::parseLine(const String &line, String (&fields)[COLUMNS]) {
  if (line.length() < 5) return false;
  for (auto &f : fields) f = "";

  int col = 0, p = 0;
  int len = (int)line.length();
  while (p < len && col < COLUMNS) {
    if (line[p] == '"') {
      // Campo entre aspas: "" dentro dele vira uma aspa.
      p++;
      String v;
      while (p < len) {
        if (line[p] == '"' && p + 1 < len && line[p + 1] == '"') {
          v += '"';
          p += 2;
        } else if (line[p] == '"') {
          p++;
          break;
        } else {
          v += line[p++];
        }
      }
      fields[col++] = v;
      if (p < len && line[p] == ',') p++;
    } else {
      int c = line.indexOf(',', p);
      if (c < 0) c = len;
      fields[col++] = line.substring(p, c);
      p = c + 1;
    }
  }
  return true;
}
