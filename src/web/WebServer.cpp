#include "web/WebServer.h"

#include <ESPAsyncWebServer.h>

#include "web/Http.h"
#include "web/Routes.h"

namespace {
AsyncWebServer s_server(80);
}

void webserver::begin() {
  routes::registerUi(s_server);
  routes::registerLibrary(s_server);
  routes::registerPlayer(s_server);
  routes::registerStats(s_server);
  routes::registerBluetooth(s_server);
  s_server.onNotFound([](AsyncWebServerRequest *req) {
    http::sendError(req, 404, "rota inexistente");
  });
  s_server.begin();
  Serial.println("[HTTP] servidor no ar");
}
