#include "web/Http.h"

namespace {
constexpr size_t MAX_BODY_BYTES = 2048;

// Com o AsyncWebServer o corpo chega em pedacos: acumula ate o ultimo e so
// entao interpreta o JSON e chama o handler.
void onBodyChunk(AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index,
                 size_t total, const http::JsonHandler &handler) {
  if (total > MAX_BODY_BYTES) {
    http::sendError(req, 413, "body grande demais");
    return;
  }
  if (index == 0) req->_tempObject = new String();
  String *buf = static_cast<String *>(req->_tempObject);
  if (!buf) {
    http::sendError(req, 500, "buffer");
    return;
  }
  buf->concat(reinterpret_cast<const char *>(data), len);
  if (index + len < total) return;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, *buf);
  delete buf;
  req->_tempObject = nullptr;
  if (err) {
    http::sendError(req, 400, "json invalido");
    return;
  }
  handler(req, doc);
}
}  // namespace

void http::sendJson(AsyncWebServerRequest *req, int code, const JsonDocument &doc) {
  String out;
  serializeJson(doc, out);
  AsyncWebServerResponse *res = req->beginResponse(code, "application/json", out);
  res->addHeader("Cache-Control", "no-store");
  req->send(res);
}

void http::sendOk(AsyncWebServerRequest *req) {
  JsonDocument d;
  d["ok"] = true;
  sendJson(req, 200, d);
}

void http::sendError(AsyncWebServerRequest *req, int code, const char *message) {
  JsonDocument d;
  d["ok"] = false;
  d["error"] = message;
  sendJson(req, code, d);
}

void http::onJsonPost(AsyncWebServer &server, const char *uri, JsonHandler handler) {
  server.on(
      uri, HTTP_POST, [](AsyncWebServerRequest *) {}, nullptr,
      [handler](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index,
                size_t total) { onBodyChunk(req, data, len, index, total, handler); });
}
