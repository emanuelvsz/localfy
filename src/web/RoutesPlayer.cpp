// Rotas do player: status (consultado a cada 1,5 s) e controles de reproducao.
#include <WiFi.h>

#include "audio/Player.h"
#include "bluetooth/Bluetooth.h"
#include "config/Config.h"
#include "core/BootMode.h"
#include "core/Lock.h"
#include "storage/Library.h"
#include "storage/MetaStore.h"
#include "storage/Storage.h"
#include "web/Http.h"
#include "web/Routes.h"

namespace {
// O status e chamado o tempo todo pela interface: precisa ser barato.
void sendStatus(AsyncWebServerRequest *req) {
  JsonDocument doc;
  doc["device"] = cfg::DEVICE_NAME;
  doc["mode"] = bootmode::current() == Mode::Bluetooth ? "bluetooth" : "wifi";

  JsonObject w = doc["wifi"].to<JsonObject>();
  w["connected"] = WiFi.isConnected();
  w["ssid"] = WiFi.SSID();
  w["ip"] = WiFi.localIP().toString();
  w["rssi"] = WiFi.RSSI();

  JsonObject b = doc["bt"].to<JsonObject>();
  b["state"] = bluetooth::stateName();
  b["name"] = bluetooth::deviceName();
  b["address"] = bluetooth::deviceAddress();

  int idx = player::currentIndex();
  String file = idx >= 0 ? library::songs()[idx].name : String("");
  const Meta *m = idx >= 0 ? meta::find(file) : nullptr;
  doc["file"] = file;
  doc["artist"] = m ? m->artist : "";
  doc["genre"] = m ? m->genre : "";
  doc["playing"] = player::isPlaying();
  doc["position"] = player::positionSecs();
  doc["duration"] = player::durationSecs();
  doc["volume"] = player::volume();
  doc["uploading"] = storage::uploading();
  doc["sdOk"] = storage::ok();

  // Durante a busca a interface le a lista de aparelhos daqui mesmo.
  if (bluetooth::state() == BtState::Scanning) {
    JsonArray d = doc["btDevices"].to<JsonArray>();
    for (auto &dev : bluetooth::devices()) {
      JsonObject o = d.add<JsonObject>();
      o["name"] = dev.name;
      o["address"] = dev.address;
      o["rssi"] = dev.rssi;
    }
  }
  http::sendJson(req, 200, doc);
}

bool requireLink(AsyncWebServerRequest *req) {
  if (bluetooth::isConnected()) return true;
  http::sendError(req, 409, "bluetooth desconectado");
  return false;
}
}  // namespace

void routes::registerPlayer(AsyncWebServer &server) {
  server.on("/api/status", HTTP_GET, sendStatus);

  http::onJsonPost(server, "/api/play", [](AsyncWebServerRequest *req, JsonDocument &doc) {
    String file = doc["file"] | "";
    Guard g;
    int idx = library::find(file);
    if (idx < 0) {
      http::sendError(req, 404, "musica inexistente");
      return;
    }
    if (!requireLink(req)) return;
    if (!player::play(idx)) {
      http::sendError(req, 500, "falha ao abrir arquivo");
      return;
    }
    http::sendOk(req);
  });

  server.on("/api/pause", HTTP_POST, [](AsyncWebServerRequest *req) {
    Guard g;
    player::pause();
    http::sendOk(req);
  });

  server.on("/api/resume", HTTP_POST, [](AsyncWebServerRequest *req) {
    Guard g;
    if (!requireLink(req)) return;
    player::resume();
    http::sendOk(req);
  });

  server.on("/api/next", HTTP_POST, [](AsyncWebServerRequest *req) {
    Guard g;
    player::next();
    http::sendOk(req);
  });

  server.on("/api/prev", HTTP_POST, [](AsyncWebServerRequest *req) {
    Guard g;
    player::previous();
    http::sendOk(req);
  });

  http::onJsonPost(server, "/api/seek", [](AsyncWebServerRequest *req, JsonDocument &doc) {
    Guard g;
    player::seek(doc["position"] | 0);
    http::sendOk(req);
  });

  http::onJsonPost(server, "/api/volume", [](AsyncWebServerRequest *req, JsonDocument &doc) {
    player::setVolume(doc["volume"] | 100);
    http::sendOk(req);
  });
}
