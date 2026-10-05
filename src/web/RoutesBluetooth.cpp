// Rotas de Bluetooth: buscar, conectar e desconectar fones.
//
// O servidor web so existe no modo Web, em que o Bluetooth esta desligado
// para caber na RAM. Por isso as rotas que ligam o radio respondem 409
// explicando como voltar ao modo Bluetooth.
#include "bluetooth/Bluetooth.h"
#include "core/BootMode.h"
#include "core/Lock.h"
#include "web/Http.h"
#include "web/Routes.h"

namespace {
bool requireBluetoothMode(AsyncWebServerRequest *req) {
  if (bootmode::current() == Mode::Bluetooth) return true;
  http::sendError(req, 409,
                  "aparelho em modo Web; aperte o botao vermelho para voltar ao Bluetooth");
  return false;
}
}  // namespace

void routes::registerBluetooth(AsyncWebServer &server) {
  // Responde na hora; a busca roda em segundo plano e a interface acompanha
  // pelo /api/status.
  http::onJsonPost(server, "/api/bt-scan", [](AsyncWebServerRequest *req, JsonDocument &doc) {
    if (!requireBluetoothMode(req)) return;
    bluetooth::requestScan(doc["duration"] | 8);
    http::sendOk(req);
  });

  server.on("/api/bt-devices", HTTP_GET, [](AsyncWebServerRequest *req) {
    JsonDocument doc;
    JsonArray arr = doc["devices"].to<JsonArray>();
    for (auto &d : bluetooth::devices()) {
      JsonObject o = arr.add<JsonObject>();
      o["name"] = d.name;
      o["address"] = d.address;
      o["rssi"] = d.rssi;
    }
    http::sendJson(req, 200, doc);
  });

  http::onJsonPost(server, "/api/bt-connect",
                   [](AsyncWebServerRequest *req, JsonDocument &doc) {
                     String name = doc["name"] | "";
                     String addr = doc["address"] | "";
                     if (name.isEmpty() && addr.isEmpty()) {
                       http::sendError(req, 400, "informe name ou address");
                       return;
                     }
                     if (!requireBluetoothMode(req)) return;
                     bluetooth::requestConnect(name, addr);
                     http::sendOk(req);
                   });

  server.on("/api/bt-disconnect", HTTP_POST, [](AsyncWebServerRequest *req) {
    Guard g;
    bluetooth::disconnect();
    http::sendOk(req);
  });
}
