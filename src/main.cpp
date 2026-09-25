/* =============================================================================
 * Localfy - Tocador de musica autonomo (ESP32)
 *
 *   SD card (MP3/WAV)  ->  decoder  ->  Bluetooth A2DP (fone/caixa)
 *   Display OLED SSD1306 (I2C) + servidor web para controle total
 *
 * Armazenamento (tudo no cartao SD, sem servidor externo):
 *   /music/                arquivos de audio
 *   /localfy/index.html    interface web
 *   /localfy/meta.json     artista + genero por musica (carregado na RAM)
 *   /localfy/history.csv   historico append-only: quando/o que/quanto tempo
 *
 * O dispositivo nao depende de nenhum PC ligado: se ha energia e cartao,
 * ha scrobble. Ver NOTAS DE ARQUITETURA no fim do arquivo.
 *
 * Build: PlatformIO, ver platformio.ini
 * ===========================================================================*/

#include <Arduino.h>
#include <WiFi.h>
#include <SPI.h>
#include <SD.h>
#include <time.h>

#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <ArduinoJson.h>

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "AudioTools.h"
#include "AudioTools/Communication/A2DPStream.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"

// ----------------------------------------------------------------------------
// Configuracao
// ----------------------------------------------------------------------------
// Credenciais ficam em src/secrets.h (fora do Git). Copie secrets.h.example.
#include "secrets.h"
static const char *WIFI_SSID = LOCALFY_WIFI_SSID;
static const char *WIFI_PASS = LOCALFY_WIFI_PASS;
static const char *DEVICE_NAME = "Localfy";

// Pinos SPI do cartao SD (VSPI). Ajuste conforme sua placa.
static const int PIN_SD_CS = 5;
static const int PIN_SD_SCK = 18;
static const int PIN_SD_MISO = 19;
static const int PIN_SD_MOSI = 23;

// OLED SSD1306 via I2C
static const int PIN_OLED_SDA = 21;
static const int PIN_OLED_SCL = 22;
static const uint8_t OLED_ADDR = 0x3C;
static const int OLED_W = 128;
static const int OLED_H = 64;

static const char *DIR_MUSIC = "/music";
static const char *DIR_DATA = "/localfy";
static const char *PATH_INDEX = "/localfy/index.html";
static const char *PATH_META = "/localfy/meta.json";
static const char *PATH_HISTORY = "/localfy/history.csv";
static const char *PATH_META_TMP = "/localfy/meta.tmp";

// Considera a musica "ouvida de verdade" a partir daqui (evita poluir o
// historico quando o usuario so passa rapido pelas faixas).
static const uint32_t SCROBBLE_MIN_SECS = 20;
static const float SCROBBLE_MIN_RATIO = 0.35f;

static const size_t MAX_SONGS = 256;
static const size_t MAX_BT_DEVICES = 20;

// Fuso horario (Brasil: UTC-3, sem horario de verao).
static const long GMT_OFFSET_SEC = -3 * 3600;
static const int DST_OFFSET_SEC = 0;

// ----------------------------------------------------------------------------
// Estado
// ----------------------------------------------------------------------------
struct Song {
  String name;
  uint32_t size;
};

struct BtDevice {
  String name;
  String address;
  int rssi;
};

enum BtState { BT_DISCONNECTED, BT_SCANNING, BT_CONNECTING, BT_CONNECTED };

// Metadados editaveis pela interface. Mantidos em RAM e persistidos em
// meta.json a cada alteracao (sao poucos bytes e raramente mudam).
struct Meta {
  String file;
  String artist;
  String genre;
};

static std::vector<Song> g_songs;
static std::vector<Meta> g_meta;
static std::vector<BtDevice> g_btDevices;

static BtState g_btState = BT_DISCONNECTED;
static String g_btName, g_btAddress;
static uint32_t g_btScanEndsAt = 0;

static int g_curIndex = -1;       // indice em g_songs
static bool g_playing = false;
static uint32_t g_duration = 0;   // segundos, 0 = desconhecido
static uint32_t g_bytesTotal = 0;
static uint8_t g_volume = 60;     // 0..100

// Contabilidade do scrobble da faixa atual.
static time_t g_playStartedAt = 0;
static uint32_t g_listenedMs = 0;
static uint32_t g_lastTickMs = 0;
static bool g_scrobbled = false;

// Uploads em andamento podem competir com o audio pelo barramento SPI do SD.
static volatile bool g_uploading = false;
static bool g_sdOk = false;
static bool g_oledOk = false;
static bool g_btRadioOn = false;
static String g_btConnectTo;

static Adafruit_SSD1306 g_oled(OLED_W, OLED_H, &Wire, -1);
static AsyncWebServer g_server(80);

// Pipeline de audio: arquivo -> decoder -> A2DP.
static A2DPStream g_a2dp;
static VolumeStream g_vol(g_a2dp);
static EncodedAudioStream g_decoder(&g_vol, new MP3DecoderHelix());
static StreamCopy g_copier;
static File g_audioFile;
static bool g_isWav = false;

// O handler do servidor web roda em outra task (AsyncTCP), entao qualquer
// escrita no SD ou no estado do player precisa ser serializada.
static SemaphoreHandle_t g_lock;
struct Guard {
  Guard() { xSemaphoreTake(g_lock, portMAX_DELAY); }
  ~Guard() { xSemaphoreGive(g_lock); }
};

// ----------------------------------------------------------------------------
// Utilidades
// ----------------------------------------------------------------------------
static String extOf(const String &name) {
  int dot = name.lastIndexOf('.');
  if (dot < 0) return "";
  String e = name.substring(dot + 1);
  e.toLowerCase();
  return e;
}

static bool isAudio(const String &name) {
  String e = extOf(name);
  return e == "mp3" || e == "wav";
}

// Rejeita travessia de diretorio: a interface so manda nomes simples.
static bool safeName(const String &name) {
  if (name.length() == 0 || name.length() > 64) return false;
  if (name.indexOf('/') >= 0 || name.indexOf('\\') >= 0) return false;
  if (name.indexOf("..") >= 0) return false;
  return isAudio(name);
}

static String musicPath(const String &name) {
  return String(DIR_MUSIC) + "/" + name;
}

static int findSong(const String &name) {
  for (size_t i = 0; i < g_songs.size(); i++)
    if (g_songs[i].name == name) return (int)i;
  return -1;
}

static Meta *findMeta(const String &file) {
  for (auto &m : g_meta)
    if (m.file == file) return &m;
  return nullptr;
}

static bool timeIsSynced() {
  return time(nullptr) > 1600000000;  // 2020-09-13, sanity check do NTP
}

// Escapa um campo para CSV (RFC 4180): aspas duplicadas, campo entre aspas.
static String csvEscape(const String &in) {
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

// ----------------------------------------------------------------------------
// Duracao
// ----------------------------------------------------------------------------
// WAV: le o header. MP3: estima pelo bitrate do primeiro frame valido, o que
// erra em arquivos VBR -- por isso a interface trata duration==0 e a barra de
// seek se desabilita sozinha quando nao da para confiar no valor.
static uint32_t wavDuration(File &f, uint32_t *byteRate) {
  uint8_t hdr[44];
  size_t pos = f.position();
  f.seek(0);
  if (f.read(hdr, 44) != 44) { f.seek(pos); return 0; }
  f.seek(pos);
  if (memcmp(hdr, "RIFF", 4) != 0 || memcmp(hdr + 8, "WAVE", 4) != 0) return 0;
  uint32_t br = hdr[28] | (hdr[29] << 8) | (hdr[30] << 16) | (hdr[31] << 24);
  if (br == 0) return 0;
  if (byteRate) *byteRate = br;
  return (f.size() - 44) / br;
}

static uint32_t mp3Duration(File &f) {
  static const int kBitrates[16] = {0,  32,  40,  48,  56,  64,  80, 96,
                                    112, 128, 160, 192, 224, 256, 320, 0};
  size_t pos = f.position();
  f.seek(0);
  uint8_t b[4];
  uint32_t scanned = 0;
  while (f.available() && scanned < 8192) {
    if (f.read(b, 1) != 1) break;
    scanned++;
    if (b[0] != 0xFF) continue;
    if (f.read(b + 1, 3) != 3) break;
    scanned += 3;
    if ((b[1] & 0xE0) != 0xE0) { f.seek(f.position() - 3); scanned -= 3; continue; }
    int brIdx = (b[2] >> 4) & 0x0F;
    int kbps = kBitrates[brIdx];
    if (kbps > 0) {
      f.seek(pos);
      return (f.size() * 8) / (kbps * 1000);
    }
  }
  f.seek(pos);
  return 0;
}

// ----------------------------------------------------------------------------
// Biblioteca
// ----------------------------------------------------------------------------
static void scanLibrary() {
  g_songs.clear();
  File dir = SD.open(DIR_MUSIC);
  if (!dir || !dir.isDirectory()) {
    Serial.printf("[SD] %s inexistente, criando\n", DIR_MUSIC);
    SD.mkdir(DIR_MUSIC);
    return;
  }
  File f;
  while ((f = dir.openNextFile()) && g_songs.size() < MAX_SONGS) {
    if (!f.isDirectory()) {
      String n = f.name();
      int slash = n.lastIndexOf('/');
      if (slash >= 0) n = n.substring(slash + 1);
      if (isAudio(n) && !n.startsWith(".")) g_songs.push_back({n, (uint32_t)f.size()});
    }
    f.close();
  }
  dir.close();

  std::sort(g_songs.begin(), g_songs.end(),
            [](const Song &a, const Song &b) { return a.name < b.name; });
  Serial.printf("[SD] %u musicas\n", (unsigned)g_songs.size());
}

// ----------------------------------------------------------------------------
// Metadados (artista / genero)
// ----------------------------------------------------------------------------
static void loadMeta() {
  g_meta.clear();
  File f = SD.open(PATH_META, FILE_READ);
  if (!f) return;
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) { Serial.printf("[META] json invalido: %s\n", err.c_str()); return; }

  for (JsonPair kv : doc.as<JsonObject>()) {
    Meta m;
    m.file = kv.key().c_str();
    m.artist = kv.value()["a"] | "";
    m.genre = kv.value()["g"] | "";
    g_meta.push_back(m);
  }
  Serial.printf("[META] %u entradas\n", (unsigned)g_meta.size());
}

// Grava em arquivo temporario e so entao substitui: se faltar energia no meio
// da escrita, o meta.json antigo continua intacto em vez de virar lixo.
static bool saveMeta() {
  JsonDocument doc;
  for (auto &m : g_meta) {
    if (m.artist.isEmpty() && m.genre.isEmpty()) continue;
    JsonObject o = doc[m.file].to<JsonObject>();
    o["a"] = m.artist;
    o["g"] = m.genre;
  }
  SD.remove(PATH_META_TMP);
  File f = SD.open(PATH_META_TMP, FILE_WRITE);
  if (!f) return false;
  bool ok = serializeJson(doc, f) > 0;
  f.flush();
  f.close();
  if (!ok) { SD.remove(PATH_META_TMP); return false; }
  SD.remove(PATH_META);
  return SD.rename(PATH_META_TMP, PATH_META);
}

static void setMeta(const String &file, const String &artist, const String &genre) {
  Meta *m = findMeta(file);
  if (!m) {
    g_meta.push_back({file, artist, genre});
  } else {
    m->artist = artist;
    m->genre = genre;
  }
  saveMeta();
}

// ----------------------------------------------------------------------------
// Historico
// ----------------------------------------------------------------------------
static void ensureHistoryHeader() {
  if (SD.exists(PATH_HISTORY)) return;
  File f = SD.open(PATH_HISTORY, FILE_WRITE);
  if (!f) return;
  f.println("timestamp,datetime,file,artist,genre,listened_secs,duration_secs,completed");
  f.close();
}

// Append-only: uma linha por musica ouvida. Nunca reescreve o arquivo, entao
// uma queda de energia custa no maximo a ultima linha.
static void writeHistory(const String &file, uint32_t listenedSecs, uint32_t durationSecs) {
  Meta *m = findMeta(file);
  time_t now = time(nullptr);

  char dt[24] = "";
  if (timeIsSynced()) {
    struct tm tmv;
    localtime_r(&now, &tmv);
    strftime(dt, sizeof(dt), "%Y-%m-%d %H:%M:%S", &tmv);
  }

  File f = SD.open(PATH_HISTORY, FILE_APPEND);
  if (!f) { Serial.println("[HIST] falha ao abrir"); return; }

  bool completed = durationSecs > 0 && listenedSecs >= (durationSecs * 9) / 10;
  f.printf("%lu,%s,%s,%s,%s,%lu,%lu,%d\n",
           timeIsSynced() ? (unsigned long)now : 0UL,
           csvEscape(dt).c_str(),
           csvEscape(file).c_str(),
           csvEscape(m ? m->artist : "").c_str(),
           csvEscape(m ? m->genre : "").c_str(),
           (unsigned long)listenedSecs,
           (unsigned long)durationSecs,
           completed ? 1 : 0);
  f.flush();
  f.close();
  Serial.printf("[HIST] %s (%lus)\n", file.c_str(), (unsigned long)listenedSecs);
}

// Grava a faixa atual no historico, se ela foi ouvida o bastante.
static void flushScrobble() {
  if (g_curIndex < 0 || g_scrobbled) return;
  uint32_t secs = g_listenedMs / 1000;
  bool enough = secs >= SCROBBLE_MIN_SECS ||
                (g_duration > 0 && secs >= g_duration * SCROBBLE_MIN_RATIO);
  if (enough) {
    writeHistory(g_songs[g_curIndex].name, secs, g_duration);
    g_scrobbled = true;
  }
}

// ----------------------------------------------------------------------------
// Player
// ----------------------------------------------------------------------------
static uint32_t positionSecs() {
  if (g_bytesTotal == 0 || g_duration == 0 || !g_audioFile) return 0;
  uint64_t pos = g_audioFile.position();
  return (uint32_t)((pos * g_duration) / g_bytesTotal);
}

static void stopPlayback() {
  flushScrobble();
  g_copier.end();
  if (g_audioFile) g_audioFile.close();
  g_playing = false;
  g_curIndex = -1;
  g_duration = g_bytesTotal = 0;
  g_listenedMs = 0;
  g_scrobbled = false;
}

static bool playIndex(int idx) {
  if (idx < 0 || idx >= (int)g_songs.size()) return false;

  flushScrobble();
  g_copier.end();
  if (g_audioFile) g_audioFile.close();

  String path = musicPath(g_songs[idx].name);
  g_audioFile = SD.open(path, FILE_READ);
  if (!g_audioFile) {
    Serial.printf("[PLAY] nao abriu %s\n", path.c_str());
    g_playing = false;
    return false;
  }

  g_isWav = extOf(g_songs[idx].name) == "wav";
  g_bytesTotal = g_audioFile.size();
  uint32_t byteRate = 0;
  g_duration = g_isWav ? wavDuration(g_audioFile, &byteRate) : mp3Duration(g_audioFile);

  // WAV cru vai direto pro A2DP; MP3 passa pelo decoder.
  if (g_isWav) {
    g_audioFile.seek(44);
    g_copier.begin(g_vol, g_audioFile);
  } else {
    g_decoder.begin();
    g_copier.begin(g_decoder, g_audioFile);
  }

  g_curIndex = idx;
  g_playing = true;
  g_listenedMs = 0;
  g_scrobbled = false;
  g_lastTickMs = millis();
  g_playStartedAt = time(nullptr);

  Serial.printf("[PLAY] %s (%lus)\n", g_songs[idx].name.c_str(), (unsigned long)g_duration);
  return true;
}

static void stepTrack(int delta) {
  if (g_songs.empty()) return;
  int n = (int)g_songs.size();
  int from = g_curIndex < 0 ? 0 : g_curIndex;
  playIndex(((from + delta) % n + n) % n);
}

static void seekTo(uint32_t secs) {
  if (!g_audioFile || g_duration == 0 || g_bytesTotal == 0) return;
  if (secs > g_duration) secs = g_duration;
  uint64_t target = ((uint64_t)secs * g_bytesTotal) / g_duration;
  if (g_isWav && target < 44) target = 44;
  g_audioFile.seek(target);
  // Pular no meio de um frame MP3 produz um estalo ate o decoder ressincronizar.
  if (!g_isWav) g_decoder.begin();
}

static void setVolume(uint8_t v) {
  if (v > 100) v = 100;
  g_volume = v;
  g_vol.setVolume(v / 100.0f);
}

// ----------------------------------------------------------------------------
// Bluetooth A2DP
// ----------------------------------------------------------------------------
// Chamado pela biblioteca para cada dispositivo encontrado. Retornar true faz
// ela conectar naquele dispositivo; false segue procurando.
static bool btSsidCallback(const char *ssid, esp_bd_addr_t addr, int rssi) {
  char mac[18];
  snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X",
           addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);

  bool known = false;
  for (auto &d : g_btDevices)
    if (d.address == mac) { known = true; break; }
  if (!known && g_btDevices.size() < MAX_BT_DEVICES) {
    g_btDevices.push_back({String(ssid), String(mac), rssi});
    Serial.printf("[BT] achou %s (%s) %d dBm\n", ssid, mac, rssi);
  }

  if (g_btConnectTo.isEmpty()) return false;  // so escaneando
  return g_btConnectTo == ssid || g_btConnectTo == mac;
}


// A biblioteca ESP32-A2DP nao expoe um modo "apenas escanear": o radio so
// liga em start(), e a descoberta acontece durante a tentativa de conexao. O
// callback abaixo registra cada dispositivo visto e responde "nao conecte",
// o que na pratica transforma o start() num scan continuo.
static void btStartScan(uint32_t seconds) {
  g_btDevices.clear();
  g_btState = BT_SCANNING;
  g_btScanEndsAt = millis() + seconds * 1000;
  g_btConnectTo = "";  // durante o scan nao queremos conectar em nada

  g_a2dp.source().set_ssid_callback(btSsidCallback);
  if (!g_btRadioOn) {
    auto cfg = g_a2dp.defaultConfig(TX_MODE);
    cfg.name = "";               // sem alvo fixo: aceita descobrir qualquer um
    cfg.auto_reconnect = false;
    g_a2dp.begin(cfg);
    g_btRadioOn = true;
  }
}

static void btConnect(const String &name, const String &address) {
  g_btState = BT_CONNECTING;
  g_btName = name;
  g_btAddress = address;
  // O callback compara com isto para decidir onde conectar.
  g_btConnectTo = name.isEmpty() ? address : name;

  g_a2dp.source().set_ssid_callback(btSsidCallback);
  if (!g_btRadioOn) {
    auto cfg = g_a2dp.defaultConfig(TX_MODE);
    cfg.name = "";
    cfg.auto_reconnect = false;
    g_a2dp.begin(cfg);
    g_btRadioOn = true;
  }
  setVolume(g_volume);
}

static void btDisconnect() {
  stopPlayback();
  g_a2dp.end();
  g_btState = BT_DISCONNECTED;
  g_btName = "";
  g_btAddress = "";
}

static void btPoll() {
  if (g_btState == BT_SCANNING && millis() > g_btScanEndsAt)
    g_btState = g_btAddress.isEmpty() ? BT_DISCONNECTED : BT_CONNECTED;

  if (g_btState == BT_CONNECTING && g_a2dp.source().is_connected())
    g_btState = BT_CONNECTED;

  if (g_btState == BT_CONNECTED && !g_a2dp.source().is_connected()) {
    // Caixa desligou ou saiu de alcance: para o audio para nao gastar CPU.
    g_btState = BT_DISCONNECTED;
    if (g_playing) { flushScrobble(); g_playing = false; }
  }
}

static const char *btStateStr() {
  switch (g_btState) {
    case BT_CONNECTED: return "connected";
    case BT_CONNECTING: return "connecting";
    case BT_SCANNING: return "scanning";
    default: return "disconnected";
  }
}

// ----------------------------------------------------------------------------
// Display OLED
// ----------------------------------------------------------------------------
static void drawDisplay() {
  if (!g_oledOk) return;
  static uint32_t last = 0;
  if (millis() - last < 500) return;  // 2 fps bastam e liberam CPU pro audio
  last = millis();

  g_oled.clearDisplay();
  g_oled.setTextColor(SSD1306_WHITE);

  g_oled.setTextSize(1);
  g_oled.setCursor(0, 0);
  g_oled.print(WiFi.isConnected() ? WiFi.localIP().toString() : "sem wifi");
  g_oled.setCursor(92, 0);
  g_oled.print(g_btState == BT_CONNECTED ? "BT ok" : "BT --");
  g_oled.drawFastHLine(0, 10, OLED_W, SSD1306_WHITE);

  if (g_curIndex >= 0) {
    String title = g_songs[g_curIndex].name;
    int dot = title.lastIndexOf('.');
    if (dot > 0) title = title.substring(0, dot);

    // Duas linhas de 21 caracteres.
    g_oled.setCursor(0, 16);
    g_oled.print(title.substring(0, 21));
    if (title.length() > 21) {
      g_oled.setCursor(0, 26);
      g_oled.print(title.substring(21, 42));
    }

    Meta *m = findMeta(g_songs[g_curIndex].name);
    if (m && !m->artist.isEmpty()) {
      g_oled.setCursor(0, 38);
      g_oled.print(m->artist.substring(0, 21));
    }

    uint32_t pos = positionSecs();
    g_oled.setCursor(0, 50);
    g_oled.printf("%s %lu:%02lu/%lu:%02lu", g_playing ? ">" : "||",
                  (unsigned long)pos / 60, (unsigned long)pos % 60,
                  (unsigned long)g_duration / 60, (unsigned long)g_duration % 60);

    if (g_duration > 0) {
      int w = (int)((uint64_t)pos * OLED_W / g_duration);
      g_oled.drawFastHLine(0, 63, w, SSD1306_WHITE);
    }
  } else {
    g_oled.setCursor(0, 28);
    g_oled.print(g_sdOk ? "Nada tocando" : "Sem cartao SD");
  }

  g_oled.display();
}

// ----------------------------------------------------------------------------
// API HTTP
// ----------------------------------------------------------------------------
static void sendJson(AsyncWebServerRequest *req, int code, const JsonDocument &doc) {
  String out;
  serializeJson(doc, out);
  AsyncWebServerResponse *res = req->beginResponse(code, "application/json", out);
  res->addHeader("Cache-Control", "no-store");
  req->send(res);
}

static void sendOk(AsyncWebServerRequest *req) {
  JsonDocument d;
  d["ok"] = true;
  sendJson(req, 200, d);
}

static void sendErr(AsyncWebServerRequest *req, int code, const char *msg) {
  JsonDocument d;
  d["ok"] = false;
  d["error"] = msg;
  sendJson(req, code, d);
}

// Le o corpo JSON de um POST. Com AsyncWebServer o corpo chega em pedacos,
// entao acumulamos ate o ultimo chunk e so ai despachamos.
using JsonHandler = std::function<void(AsyncWebServerRequest *, JsonDocument &)>;

static void onJsonBody(AsyncWebServerRequest *req, uint8_t *data, size_t len,
                       size_t index, size_t total, JsonHandler fn) {
  if (total > 2048) { sendErr(req, 413, "body grande demais"); return; }
  if (index == 0) req->_tempObject = new String();
  String *buf = (String *)req->_tempObject;
  if (!buf) { sendErr(req, 500, "buffer"); return; }
  buf->concat((const char *)data, len);
  if (index + len < total) return;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, *buf);
  delete buf;
  req->_tempObject = nullptr;
  if (err) { sendErr(req, 400, "json invalido"); return; }
  fn(req, doc);
}

static void registerRoutes() {
  // ---- interface ----
  // Quando a interface ainda nao esta no cartao, servimos uma pagina minima
  // que permite enviar o index.html pela rede -- assim nao e preciso tirar o
  // cartao do aparelho para atualizar a interface.
  g_server.on("/", HTTP_GET, [](AsyncWebServerRequest *req) {
    if (g_sdOk && SD.exists(PATH_INDEX)) {
      req->send(SD, PATH_INDEX, "text/html");
      return;
    }
    String p = F("<!doctype html><meta charset=utf-8>"
                 "<meta name=viewport content='width=device-width,initial-scale=1'>"
                 "<title>Localfy - instalar interface</title>"
                 "<body style='background:#121212;color:#fff;font-family:system-ui,sans-serif;"
                 "padding:32px;max-width:520px;margin:auto'>"
                 "<h2 style='color:#1DB954'>Localfy</h2>");
    if (!g_sdOk) {
      p += F("<p>Cartao SD nao detectado. Verifique a fiacao e reinicie.</p>");
    } else {
      p += F("<p>A interface ainda nao esta instalada no cartao.</p>"
             "<p>Selecione o arquivo <code>index.html</code> do projeto:</p>"
             "<form method=POST action='/api/install-ui' enctype='multipart/form-data'>"
             "<input type=file name=file accept='.html' required "
             "style='display:block;margin:18px 0;color:#b3b3b3'>"
             "<button style='background:#1DB954;color:#000;font-weight:700;border:0;"
             "padding:12px 22px;border-radius:99px;font-size:15px;cursor:pointer'>"
             "Instalar interface</button></form>");
    }
    p += F("</body>");
    req->send(200, "text/html", p);
  });

  // Recebe o index.html e grava em /localfy/ no cartao.
  g_server.on("/api/install-ui", HTTP_POST,
    [](AsyncWebServerRequest *req) {
      req->send(200, "text/html",
                "<meta charset=utf-8><body style='background:#121212;color:#fff;"
                "font-family:system-ui,sans-serif;padding:32px'>"
                "<h2 style='color:#1DB954'>Interface instalada</h2>"
                "<p><a href='/' style='color:#1DB954'>Abrir o Localfy</a></p>");
    },
    [](AsyncWebServerRequest *req, String filename, size_t index, uint8_t *data,
       size_t len, bool final) {
      static File ui;
      if (index == 0) {
        if (!g_sdOk) { req->send(503, "text/plain", "sem cartao"); return; }
        SD.mkdir(DIR_DATA);
        SD.remove(PATH_INDEX);
        ui = SD.open(PATH_INDEX, FILE_WRITE);
        if (!ui) { req->send(500, "text/plain", "falha ao criar"); return; }
        Serial.println("[UI] recebendo index.html");
      }
      if (ui && len) ui.write(data, len);
      if (final && ui) {
        ui.flush();
        Serial.printf("[UI] instalada (%u bytes)\n", (unsigned)(index + len));
        ui.close();
      }
    });

  // ---- biblioteca ----
  g_server.on("/api/songs", HTTP_GET, [](AsyncWebServerRequest *req) {
    if (!g_sdOk) { sendErr(req, 503, "sem cartao SD"); return; }
    JsonDocument doc;
    JsonArray arr = doc["songs"].to<JsonArray>();
    for (auto &s : g_songs) {
      JsonObject o = arr.add<JsonObject>();
      o["name"] = s.name;
      o["size"] = s.size;
      Meta *m = findMeta(s.name);
      o["artist"] = m ? m->artist : "";
      o["genre"] = m ? m->genre : "";
    }
    sendJson(req, 200, doc);
  });

  // ---- status (chamado a cada 1.5s: precisa ser barato) ----
  g_server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *req) {
    JsonDocument doc;
    doc["device"] = DEVICE_NAME;

    JsonObject w = doc["wifi"].to<JsonObject>();
    w["connected"] = WiFi.isConnected();
    w["ssid"] = WiFi.SSID();
    w["ip"] = WiFi.localIP().toString();
    w["rssi"] = WiFi.RSSI();

    JsonObject b = doc["bt"].to<JsonObject>();
    b["state"] = btStateStr();
    b["name"] = g_btName;
    b["address"] = g_btAddress;

    if (g_curIndex >= 0) {
      const String &file = g_songs[g_curIndex].name;
      doc["file"] = file;
      Meta *m = findMeta(file);
      doc["artist"] = m ? m->artist : "";
      doc["genre"] = m ? m->genre : "";
    } else {
      doc["file"] = "";
      doc["artist"] = "";
      doc["genre"] = "";
    }
    doc["playing"] = g_playing;
    doc["position"] = positionSecs();
    doc["duration"] = g_duration;
    doc["volume"] = g_volume;
    doc["uploading"] = g_uploading;
    doc["sdOk"] = g_sdOk;

    // Durante o scan a interface le a lista daqui mesmo, sem chamada extra.
    if (g_btState == BT_SCANNING) {
      JsonArray d = doc["btDevices"].to<JsonArray>();
      for (auto &dev : g_btDevices) {
        JsonObject o = d.add<JsonObject>();
        o["name"] = dev.name;
        o["address"] = dev.address;
        o["rssi"] = dev.rssi;
      }
    }
    sendJson(req, 200, doc);
  });

  // ---- transporte ----
  g_server.on("/api/play", HTTP_POST, [](AsyncWebServerRequest *req) {}, nullptr,
    [](AsyncWebServerRequest *req, uint8_t *d, size_t l, size_t i, size_t t) {
      onJsonBody(req, d, l, i, t, [](AsyncWebServerRequest *req, JsonDocument &doc) {
        String file = doc["file"] | "";
        Guard g;
        int idx = findSong(file);
        if (idx < 0) { sendErr(req, 404, "musica inexistente"); return; }
        if (g_btState != BT_CONNECTED) { sendErr(req, 409, "bluetooth desconectado"); return; }
        if (!playIndex(idx)) { sendErr(req, 500, "falha ao abrir arquivo"); return; }
        sendOk(req);
      });
    });

  g_server.on("/api/pause", HTTP_POST, [](AsyncWebServerRequest *req) {
    Guard g;
    g_playing = false;
    sendOk(req);
  });

  g_server.on("/api/resume", HTTP_POST, [](AsyncWebServerRequest *req) {
    Guard g;
    if (g_btState != BT_CONNECTED) { sendErr(req, 409, "bluetooth desconectado"); return; }
    if (g_curIndex < 0) stepTrack(0);
    else { g_playing = true; g_lastTickMs = millis(); }
    sendOk(req);
  });

  g_server.on("/api/next", HTTP_POST, [](AsyncWebServerRequest *req) {
    Guard g; stepTrack(1); sendOk(req);
  });

  g_server.on("/api/prev", HTTP_POST, [](AsyncWebServerRequest *req) {
    Guard g;
    // Mesma convencao do Spotify: reinicia se ja passou dos 3s.
    if (positionSecs() > 3) seekTo(0);
    else stepTrack(-1);
    sendOk(req);
  });

  g_server.on("/api/seek", HTTP_POST, [](AsyncWebServerRequest *req) {}, nullptr,
    [](AsyncWebServerRequest *req, uint8_t *d, size_t l, size_t i, size_t t) {
      onJsonBody(req, d, l, i, t, [](AsyncWebServerRequest *req, JsonDocument &doc) {
        Guard g;
        seekTo(doc["position"] | 0);
        sendOk(req);
      });
    });

  g_server.on("/api/volume", HTTP_POST, [](AsyncWebServerRequest *req) {}, nullptr,
    [](AsyncWebServerRequest *req, uint8_t *d, size_t l, size_t i, size_t t) {
      onJsonBody(req, d, l, i, t, [](AsyncWebServerRequest *req, JsonDocument &doc) {
        setVolume(doc["volume"] | 60);
        sendOk(req);
      });
    });

  // ---- metadados ----
  g_server.on("/api/meta", HTTP_POST, [](AsyncWebServerRequest *req) {}, nullptr,
    [](AsyncWebServerRequest *req, uint8_t *d, size_t l, size_t i, size_t t) {
      onJsonBody(req, d, l, i, t, [](AsyncWebServerRequest *req, JsonDocument &doc) {
        String file = doc["file"] | "";
        if (!safeName(file)) { sendErr(req, 400, "nome invalido"); return; }
        Guard g;
        if (findSong(file) < 0) { sendErr(req, 404, "musica inexistente"); return; }
        String artist = doc["artist"] | "";
        String genre = doc["genre"] | "";
        artist = artist.substring(0, 64);
        genre = genre.substring(0, 32);
        setMeta(file, artist, genre);
        sendOk(req);
      });
    });

  // Lista de generos ja usados, para a interface sugerir em autocomplete.
  g_server.on("/api/genres", HTTP_GET, [](AsyncWebServerRequest *req) {
    JsonDocument doc;
    JsonArray arr = doc["genres"].to<JsonArray>();
    for (auto &m : g_meta) {
      if (m.genre.isEmpty()) continue;
      bool dup = false;
      for (JsonVariant v : arr)
        if (m.genre == v.as<const char *>()) { dup = true; break; }
      if (!dup) arr.add(m.genre);
    }
    sendJson(req, 200, doc);
  });

  // ---- historico ----
  g_server.on("/api/history.csv", HTTP_GET, [](AsyncWebServerRequest *req) {
    if (!SD.exists(PATH_HISTORY)) { sendErr(req, 404, "sem historico"); return; }
    AsyncWebServerResponse *res = req->beginResponse(SD, PATH_HISTORY, "text/csv");
    res->addHeader("Content-Disposition", "attachment; filename=\"localfy-history.csv\"");
    req->send(res);
  });

  // Estatisticas agregadas. Varre o CSV uma vez, linha a linha, sem carregar
  // o arquivo inteiro na RAM (ele cresce para sempre).
  // Estatisticas de um mes. Recebe {"month":N} onde 0 = mes atual, 1 = o
  // anterior e assim por diante. Varre o CSV uma vez, linha a linha: o
  // arquivo cresce para sempre e nao cabe na RAM.
  g_server.on("/api/stats-monthly", HTTP_POST, [](AsyncWebServerRequest *req) {}, nullptr,
    [](AsyncWebServerRequest *req, uint8_t *dta, size_t l, size_t i, size_t t) {
      onJsonBody(req, dta, l, i, t, [](AsyncWebServerRequest *req, JsonDocument &body) {
        int back = body["month"] | 0;
        if (back < 0) back = 0;
        if (back > 240) back = 240;

        if (!timeIsSynced()) { sendErr(req, 503, "relogio sem sincronia"); return; }

        // Determina o mes alvo recuando 'back' meses a partir de hoje.
        time_t now = time(nullptr);
        struct tm ref;
        localtime_r(&now, &ref);
        int ty = ref.tm_year, tm_ = ref.tm_mon - back;
        while (tm_ < 0) { tm_ += 12; ty--; }

        static const int mdays[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
        int year = ty + 1900;
        int days = mdays[tm_];
        if (tm_ == 1 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)) days = 29;

        uint16_t byDay[31] = {0};
        uint16_t byHour[24] = {0};
        uint32_t plays = 0, secs = 0;
        std::vector<std::pair<String, uint32_t>> artists, genres, tracks;

        File f = SD.open(PATH_HISTORY, FILE_READ);
        if (f) {
          f.readStringUntil('\n');  // cabecalho
          while (f.available()) {
            String line = f.readStringUntil('\n');
            if (line.length() < 5) continue;

            String fields[8];
            int col = 0, p = 0;
            while (p < (int)line.length() && col < 8) {
              if (line[p] == '"') {
                p++;
                String v;
                while (p < (int)line.length()) {
                  if (line[p] == '"' && p + 1 < (int)line.length() && line[p+1] == '"') { v += '"'; p += 2; }
                  else if (line[p] == '"') { p++; break; }
                  else v += line[p++];
                }
                fields[col++] = v;
                if (p < (int)line.length() && line[p] == ',') p++;
              } else {
                int c = line.indexOf(',', p);
                if (c < 0) c = line.length();
                fields[col++] = line.substring(p, c);
                p = c + 1;
              }
            }

            time_t ts = (time_t)fields[0].toInt();
            if (ts <= 0) continue;              // linha gravada sem relogio
            struct tm lt;
            localtime_r(&ts, &lt);
            if (lt.tm_year != ty || lt.tm_mon != tm_) continue;

            int dom = lt.tm_mday - 1;
            if (dom >= 0 && dom < 31) byDay[dom]++;
            if (lt.tm_hour >= 0 && lt.tm_hour < 24) byHour[lt.tm_hour]++;
            plays++;
            secs += fields[5].toInt();

            auto bump = [](std::vector<std::pair<String, uint32_t>> &v, const String &k) {
              if (k.isEmpty()) return;
              for (auto &e : v) if (e.first == k) { e.second++; return; }
              if (v.size() < 48) v.push_back({k, 1});
            };
            bump(artists, fields[3]);
            bump(genres, fields[4]);
            bump(tracks, fields[2]);
          }
          f.close();
        }

        JsonDocument doc;
        doc["year"] = year;
        doc["month"] = tm_;
        doc["days"] = days;
        static const char *MES[12] = {"janeiro","fevereiro","marco","abril","maio",
          "junho","julho","agosto","setembro","outubro","novembro","dezembro"};
        doc["label"] = String(MES[tm_]) + " de " + String(year);
        doc["plays"] = plays;
        doc["seconds"] = secs;

        JsonArray ad = doc["byDay"].to<JsonArray>();
        for (int d = 0; d < days; d++) ad.add(byDay[d]);
        JsonArray ah = doc["byHour"].to<JsonArray>();
        for (int h = 0; h < 24; h++) ah.add(byHour[h]);

        auto topN = [&doc](const char *key, std::vector<std::pair<String, uint32_t>> &v) {
          std::sort(v.begin(), v.end(),
                    [](const std::pair<String,uint32_t> &a, const std::pair<String,uint32_t> &b) {
                      return a.second > b.second;
                    });
          JsonArray arr = doc[key].to<JsonArray>();
          for (size_t i = 0; i < v.size() && i < 5; i++) {
            JsonObject o = arr.add<JsonObject>();
            o["name"] = v[i].first;
            o["plays"] = v[i].second;
          }
        };
        topN("topArtists", artists);
        topN("topGenres", genres);
        topN("topTracks", tracks);
        sendJson(req, 200, doc);
      });
    });

  g_server.on("/api/stats", HTTP_GET, [](AsyncWebServerRequest *req) {
    JsonDocument doc;
    uint32_t plays = 0, secs = 0;
    std::vector<std::pair<String, uint32_t>> artists, genres;

    File f = SD.open(PATH_HISTORY, FILE_READ);
    if (f) {
      f.readStringUntil('\n');  // descarta cabecalho
      while (f.available()) {
        String line = f.readStringUntil('\n');
        if (line.length() < 5) continue;

        // Colunas: ts,datetime,file,artist,genre,listened,duration,completed
        // datetime/file/artist/genre vem entre aspas (csvEscape).
        int col = 0, p = 0;
        String fields[8];
        while (p < (int)line.length() && col < 8) {
          if (line[p] == '"') {
            p++;
            String v;
            while (p < (int)line.length()) {
              if (line[p] == '"' && p + 1 < (int)line.length() && line[p + 1] == '"') {
                v += '"'; p += 2;
              } else if (line[p] == '"') { p++; break; }
              else v += line[p++];
            }
            fields[col++] = v;
            if (p < (int)line.length() && line[p] == ',') p++;
          } else {
            int c = line.indexOf(',', p);
            if (c < 0) c = line.length();
            fields[col++] = line.substring(p, c);
            p = c + 1;
          }
        }
        plays++;
        secs += fields[5].toInt();

        auto bump = [](std::vector<std::pair<String, uint32_t>> &v, const String &k) {
          if (k.isEmpty()) return;
          for (auto &e : v) if (e.first == k) { e.second++; return; }
          if (v.size() < 64) v.push_back({k, 1});
        };
        bump(artists, fields[3]);
        bump(genres, fields[4]);
      }
      f.close();
    }

    auto topN = [&doc](const char *key, std::vector<std::pair<String, uint32_t>> &v) {
      std::sort(v.begin(), v.end(),
                [](const std::pair<String, uint32_t> &a, const std::pair<String, uint32_t> &b) {
                  return a.second > b.second;
                });
      JsonArray arr = doc[key].to<JsonArray>();
      for (size_t i = 0; i < v.size() && i < 10; i++) {
        JsonObject o = arr.add<JsonObject>();
        o["name"] = v[i].first;
        o["plays"] = v[i].second;
      }
    };
    doc["plays"] = plays;
    doc["seconds"] = secs;
    topN("topArtists", artists);
    topN("topGenres", genres);
    sendJson(req, 200, doc);
  });

  // ---- arquivos ----
  g_server.on("/api/delete", HTTP_POST, [](AsyncWebServerRequest *req) {}, nullptr,
    [](AsyncWebServerRequest *req, uint8_t *d, size_t l, size_t i, size_t t) {
      onJsonBody(req, d, l, i, t, [](AsyncWebServerRequest *req, JsonDocument &doc) {
        String file = doc["file"] | "";
        if (!safeName(file)) { sendErr(req, 400, "nome invalido"); return; }
        Guard g;
        int idx = findSong(file);
        if (idx < 0) { sendErr(req, 404, "musica inexistente"); return; }
        if (idx == g_curIndex) stopPlayback();  // libera o handle antes de remover
        if (!SD.remove(musicPath(file))) { sendErr(req, 500, "falha ao remover"); return; }
        scanLibrary();
        sendOk(req);
      });
    });

  g_server.on("/api/upload", HTTP_POST,
    [](AsyncWebServerRequest *req) {
      g_uploading = false;
      scanLibrary();
      sendOk(req);
    },
    // Chunks do multipart chegam aqui. Gravamos direto no SD: nada de
    // bufferizar o arquivo na RAM, que nao caberia.
    [](AsyncWebServerRequest *req, String filename, size_t index, uint8_t *data,
       size_t len, bool final) {
      static File up;
      if (index == 0) {
        if (!safeName(filename)) { req->send(400, "text/plain", "nome invalido"); return; }
        g_uploading = true;
        String path = musicPath(filename);
        SD.remove(path);
        up = SD.open(path, FILE_WRITE);
        if (!up) { g_uploading = false; req->send(500, "text/plain", "falha ao criar"); return; }
        Serial.printf("[UP] %s\n", filename.c_str());
      }
      if (up && len) up.write(data, len);
      if (final) {
        if (up) { up.flush(); up.close(); }
        g_uploading = false;
      }
    });

  // ---- bluetooth ----
  g_server.on("/api/bt-scan", HTTP_POST, [](AsyncWebServerRequest *req) {}, nullptr,
    [](AsyncWebServerRequest *req, uint8_t *d, size_t l, size_t i, size_t t) {
      onJsonBody(req, d, l, i, t, [](AsyncWebServerRequest *req, JsonDocument &doc) {
        // Responde na hora; o scan roda em background e a interface acompanha
        // pelo /api/status.
        btStartScan(doc["duration"] | 8);
        sendOk(req);
      });
    });

  g_server.on("/api/bt-devices", HTTP_GET, [](AsyncWebServerRequest *req) {
    JsonDocument doc;
    JsonArray arr = doc["devices"].to<JsonArray>();
    for (auto &d : g_btDevices) {
      JsonObject o = arr.add<JsonObject>();
      o["name"] = d.name;
      o["address"] = d.address;
      o["rssi"] = d.rssi;
    }
    sendJson(req, 200, doc);
  });

  g_server.on("/api/bt-connect", HTTP_POST, [](AsyncWebServerRequest *req) {}, nullptr,
    [](AsyncWebServerRequest *req, uint8_t *d, size_t l, size_t i, size_t t) {
      onJsonBody(req, d, l, i, t, [](AsyncWebServerRequest *req, JsonDocument &doc) {
        String name = doc["name"] | "";
        String addr = doc["address"] | "";
        if (name.isEmpty() && addr.isEmpty()) { sendErr(req, 400, "informe name ou address"); return; }
        btConnect(name, addr);
        sendOk(req);
      });
    });

  g_server.on("/api/bt-disconnect", HTTP_POST, [](AsyncWebServerRequest *req) {
    Guard g;
    btDisconnect();
    sendOk(req);
  });

  g_server.onNotFound([](AsyncWebServerRequest *req) { sendErr(req, 404, "rota inexistente"); });
}

// ----------------------------------------------------------------------------
// setup / loop
// ----------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n=== Localfy ===");
  // A causa do ultimo reset costuma explicar boot loop: BROWNOUT = queda de
  // tensao (fonte fraca), PANIC = excecao no codigo.
  Serial.printf("[BOOT] motivo do reset: %d (1=power-on 3=sw 6=brownout 12=panic)\n",
                (int)esp_reset_reason());

  g_lock = xSemaphoreCreateMutex();

  Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
  // Sonda o endereco antes: Adafruit_SSD1306::begin() pode ficar preso quando
  // nao ha display no barramento, e ai o aparelho nunca chega a subir o Wi-Fi.
  Wire.beginTransmission(OLED_ADDR);
  bool oledPresent = (Wire.endTransmission() == 0);
  if (!oledPresent) Serial.println("[OLED] nao detectado no barramento I2C");
  g_oledOk = oledPresent && g_oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  if (g_oledOk) {
    g_oled.clearDisplay();
    g_oled.setTextColor(SSD1306_WHITE);
    g_oled.setTextSize(1);
    g_oled.setCursor(0, 28);
    g_oled.print("Localfy iniciando");
    g_oled.display();
  } else if (oledPresent) {
    Serial.println("[OLED] detectado mas begin() falhou");
  }

  SPI.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
  // Sem cartao nao da para tocar nada, mas ainda vale subir o Wi-Fi e a
  // interface: assim da para diagnosticar pela rede em vez de ficar so no
  // monitor serial. g_sdOk gateia tudo que depende do cartao.
  // SD.begin() pode retornar true mesmo com o cartao respondendo mal, entao
  // so consideramos ok se o tipo do cartao for valido.
  g_sdOk = SD.begin(PIN_SD_CS) && SD.cardType() != CARD_NONE;
  if (g_sdOk) {
    SD.mkdir(DIR_DATA);
    scanLibrary();
    loadMeta();
    ensureHistoryHeader();
    uint8_t t = SD.cardType();
    Serial.printf("[SD] ok - tipo %s, %llu MB\n",
                  t == CARD_MMC ? "MMC" : t == CARD_SD ? "SD" : t == CARD_SDHC ? "SDHC" : "?",
                  SD.cardSize() / (1024ULL * 1024ULL));
  } else {
    Serial.println("[SD] FALHOU ao montar. Checar:");
    Serial.printf("  1. Fiacao: CS=%d SCK=%d MISO=%d MOSI=%d\n",
                  PIN_SD_CS, PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI);
    Serial.println("  2. Alimentacao do modulo (a maioria quer 5V no VCC)");
    Serial.println("  3. Cartao formatado em FAT32 (nao exFAT/NTFS)");
    Serial.println("  4. Cartao bem encaixado no soquete");
  }

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);  // sleep do Wi-Fi causa engasgo no A2DP
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("[WIFI] conectando");
  for (int i = 0; i < 40 && !WiFi.isConnected(); i++) { delay(250); Serial.print('.'); }
  Serial.println();
  if (WiFi.isConnected()) {
    Serial.printf("[WIFI] %s\n", WiFi.localIP().toString().c_str());
    configTime(GMT_OFFSET_SEC, DST_OFFSET_SEC, "pool.ntp.org", "a.st1.ntp.br");
  } else {
    Serial.println("[WIFI] falhou - seguindo offline");
  }

  g_decoder.begin();
  g_vol.begin();
  setVolume(g_volume);

  registerRoutes();
  g_server.begin();
  Serial.println("[HTTP] servidor no ar");
}

void loop() {
  btPoll();

  if (g_playing && g_btState == BT_CONNECTED) {
    Guard g;
    // Empurra audio para o A2DP. copy() retorna 0 no fim do arquivo.
    size_t moved = g_copier.copy();

    uint32_t now = millis();
    g_listenedMs += now - g_lastTickMs;
    g_lastTickMs = now;

    if (moved == 0) {
      // Fim da faixa: registra e vai para a proxima.
      g_listenedMs = g_duration > 0 ? g_duration * 1000 : g_listenedMs;
      flushScrobble();
      stepTrack(1);
    }
  } else {
    g_lastTickMs = millis();
    delay(5);
  }

  drawDisplay();
}

/* =============================================================================
 * NOTAS DE ARQUITETURA
 *
 * 1. POR QUE NAO TEM BANCO DE DADOS / DOCKER
 *    Um Postgres em container so grava enquanto o PC estiver ligado. Como o
 *    aparelho e autonomo, o historico ficaria cheio de buracos justamente nas
 *    horas em que ele e mais usado (PC desligado, tocando musica na sala).
 *    Aqui o CSV no proprio cartao garante que todo play vira registro. Se
 *    depois voce quiser analise pesada, baixe o CSV em /api/history.csv e
 *    jogue num Postgres/pandas -- o dado bruto nunca fica preso no device.
 *
 * 2. SPI DO SD x A2DP
 *    Upload e reproducao disputam o mesmo barramento SPI. Em cartoes lentos o
 *    audio pode engasgar durante um upload grande; /api/status expoe o campo
 *    "uploading" caso voce queira avisar isso na interface.
 *
 * 3. DURACAO DE MP3
 *    mp3Duration() le o bitrate do primeiro frame e extrapola. Em arquivos VBR
 *    o valor erra. A interface ja lida com duration==0 desabilitando o seek, e
 *    essa e a saida honesta quando nao da para saber.
 *
 * 4. SEM PSRAM
 *    g_songs e g_meta ficam na RAM interna, limitados a MAX_SONGS. Para
 *    bibliotecas maiores, uma placa com PSRAM (ESP32-WROVER) e o caminho.
 * ===========================================================================*/
