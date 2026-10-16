// Rotas de historico e estatisticas.
//
// As estatisticas varrem o history.csv linha a linha, sem carregar o arquivo
// na RAM: ele cresce para sempre e nao caberia.
#include <SD.h>
#include <sys/time.h>
#include <time.h>

#include <algorithm>
#include <utility>
#include <vector>

#include "config/Config.h"
#include "core/TimeUtil.h"
#include "storage/History.h"
#include "web/Http.h"
#include "web/Routes.h"

namespace {
// Contador "nome -> quantidade" com limite de entradas (protege a RAM).
using Counter = std::vector<std::pair<String, uint32_t>>;

void bump(Counter &c, const String &key, size_t limit) {
  if (key.isEmpty()) return;
  for (auto &e : c)
    if (e.first == key) {
      e.second++;
      return;
    }
  if (c.size() < limit) c.push_back({key, 1});
}

void writeTop(JsonDocument &doc, const char *key, Counter &c, size_t n) {
  std::sort(c.begin(), c.end(),
            [](const std::pair<String, uint32_t> &a, const std::pair<String, uint32_t> &b) {
              return a.second > b.second;
            });
  JsonArray arr = doc[key].to<JsonArray>();
  for (size_t i = 0; i < c.size() && i < n; i++) {
    JsonObject o = arr.add<JsonObject>();
    o["name"] = c[i].first;
    o["plays"] = c[i].second;
  }
}

// Percorre cada linha valida do historico chamando 'fn' com as colunas.
template <typename Fn>
void forEachHistoryLine(Fn fn) {
  File f = SD.open(cfg::path::HISTORY, FILE_READ);
  if (!f) return;
  f.readStringUntil('\n');   // cabecalho
  String fields[history::COLUMNS];
  while (f.available()) {
    String line = f.readStringUntil('\n');
    if (history::parseLine(line, fields)) fn(fields);
  }
  f.close();
}

// Totais de todo o historico.
void sendAllTime(AsyncWebServerRequest *req) {
  uint32_t plays = 0, secs = 0;
  Counter artists, genres;
  forEachHistoryLine([&](String(&f)[history::COLUMNS]) {
    plays++;
    secs += f[history::LISTENED].toInt();
    bump(artists, f[history::ARTIST], 64);
    bump(genres, f[history::GENRE], 64);
  });

  JsonDocument doc;
  doc["plays"] = plays;
  doc["seconds"] = secs;
  writeTop(doc, "topArtists", artists, 10);
  writeTop(doc, "topGenres", genres, 10);
  http::sendJson(req, 200, doc);
}

// Estatisticas de um mes. {"month":N}: 0 = mes atual, 1 = anterior...
void sendMonthly(AsyncWebServerRequest *req, JsonDocument &body) {
  int back = body["month"] | 0;
  back = std::max(0, std::min(back, 240));
  if (!timeutil::synced()) {
    http::sendError(req, 503, "relogio sem sincronia");
    return;
  }

  // Mes alvo: recua 'back' meses a partir de hoje.
  time_t now = time(nullptr);
  struct tm ref;
  localtime_r(&now, &ref);
  int targetYear = ref.tm_year, targetMonth = ref.tm_mon - back;
  while (targetMonth < 0) {
    targetMonth += 12;
    targetYear--;
  }

  static const int kDaysInMonth[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  int year = targetYear + 1900;
  int days = kDaysInMonth[targetMonth];
  bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
  if (targetMonth == 1 && leap) days = 29;

  uint16_t byDay[31] = {0};
  uint16_t byHour[24] = {0};
  uint32_t plays = 0, secs = 0;
  Counter artists, genres, tracks;

  forEachHistoryLine([&](String(&f)[history::COLUMNS]) {
    time_t ts = (time_t)f[history::TIMESTAMP].toInt();
    if (ts <= 0) return;   // linha gravada sem relogio
    struct tm lt;
    localtime_r(&ts, &lt);
    if (lt.tm_year != targetYear || lt.tm_mon != targetMonth) return;

    int dom = lt.tm_mday - 1;
    if (dom >= 0 && dom < 31) byDay[dom]++;
    if (lt.tm_hour >= 0 && lt.tm_hour < 24) byHour[lt.tm_hour]++;
    plays++;
    secs += f[history::LISTENED].toInt();
    bump(artists, f[history::ARTIST], 48);
    bump(genres, f[history::GENRE], 48);
    bump(tracks, f[history::FILE_NAME], 48);
  });

  static const char *kMonthNames[12] = {"janeiro", "fevereiro", "marco",    "abril",
                                        "maio",    "junho",     "julho",    "agosto",
                                        "setembro", "outubro",  "novembro", "dezembro"};
  JsonDocument doc;
  doc["year"] = year;
  doc["month"] = targetMonth;
  doc["days"] = days;
  doc["label"] = String(kMonthNames[targetMonth]) + " de " + String(year);
  doc["plays"] = plays;
  doc["seconds"] = secs;
  JsonArray ad = doc["byDay"].to<JsonArray>();
  for (int d = 0; d < days; d++) ad.add(byDay[d]);
  JsonArray ah = doc["byHour"].to<JsonArray>();
  for (int h = 0; h < 24; h++) ah.add(byHour[h]);
  writeTop(doc, "topArtists", artists, 5);
  writeTop(doc, "topGenres", genres, 5);
  writeTop(doc, "topTracks", tracks, 5);
  http::sendJson(req, 200, doc);
}
}  // namespace

void routes::registerStats(AsyncWebServer &server) {
  // A rede do Localfy nao tem internet (sem NTP): a interface manda a hora do
  // celular ao abrir, para o historico ter data.
  http::onJsonPost(server, "/api/time", [](AsyncWebServerRequest *req, JsonDocument &doc) {
    long epoch = doc["epoch"] | 0L;
    if (epoch < 1600000000L) {
      http::sendError(req, 400, "hora invalida");
      return;
    }
    timeval tv{epoch, 0};
    settimeofday(&tv, nullptr);
    // Fuso fixo de Config.h, no formato POSIX ("<-03>3" para UTC-3).
    char tz[16];
    int h = (int)(cfg::net::GMT_OFFSET_SEC / 3600);
    snprintf(tz, sizeof(tz), "<%+03d>%d", h, -h);
    setenv("TZ", tz, 1);
    tzset();
    Serial.printf("[TIME] relogio acertado pela interface (%ld)\n", epoch);
    http::sendOk(req);
  });

  server.on("/api/history.csv", HTTP_GET, [](AsyncWebServerRequest *req) {
    if (!SD.exists(cfg::path::HISTORY)) {
      http::sendError(req, 404, "sem historico");
      return;
    }
    AsyncWebServerResponse *res = req->beginResponse(SD, cfg::path::HISTORY, "text/csv");
    res->addHeader("Content-Disposition", "attachment; filename=\"localfy-history.csv\"");
    req->send(res);
  });

  server.on("/api/stats", HTTP_GET, sendAllTime);
  http::onJsonPost(server, "/api/stats-monthly", sendMonthly);
}
