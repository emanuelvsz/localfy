#pragma once
// Utilidades para as rotas da API: respostas JSON e leitura de corpo JSON.
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>

#include <functional>

namespace http {
void sendJson(AsyncWebServerRequest *req, int code, const JsonDocument &doc);
void sendOk(AsyncWebServerRequest *req);
void sendError(AsyncWebServerRequest *req, int code, const char *message);

using JsonHandler = std::function<void(AsyncWebServerRequest *, JsonDocument &)>;
// Registra uma rota POST cujo corpo e JSON. O handler recebe o JSON ja lido.
void onJsonPost(AsyncWebServer &server, const char *uri, JsonHandler handler);
}  // namespace http
