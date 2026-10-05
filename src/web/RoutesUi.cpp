// Rotas da interface: a pagina principal e a instalacao do index.html.
#include <SD.h>

#include "config/Config.h"
#include "storage/Storage.h"
#include "web/Routes.h"

namespace {
// Pagina minima mostrada enquanto a interface nao esta no cartao: permite
// enviar o index.html pela rede, sem tirar o cartao do aparelho.
String installPage() {
  String p = F("<!doctype html><meta charset=utf-8>"
               "<meta name=viewport content='width=device-width,initial-scale=1'>"
               "<title>Localfy - instalar interface</title>"
               "<body style='background:#121212;color:#fff;font-family:system-ui,sans-serif;"
               "padding:32px;max-width:520px;margin:auto'>"
               "<h2 style='color:#1DB954'>Localfy</h2>");
  if (!storage::ok()) {
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
  return p;
}
}  // namespace

void routes::registerUi(AsyncWebServer &server) {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *req) {
    if (storage::ok() && SD.exists(cfg::path::UI_INDEX)) {
      req->send(SD, cfg::path::UI_INDEX, "text/html");
      return;
    }
    req->send(200, "text/html", installPage());
  });

  // Recebe o index.html e grava em /localfy/ no cartao.
  server.on(
      "/api/install-ui", HTTP_POST,
      [](AsyncWebServerRequest *req) {
        req->send(200, "text/html",
                  "<meta charset=utf-8><body style='background:#121212;color:#fff;"
                  "font-family:system-ui,sans-serif;padding:32px'>"
                  "<h2 style='color:#1DB954'>Interface instalada</h2>"
                  "<p><a href='/' style='color:#1DB954'>Abrir o Localfy</a></p>");
      },
      [](AsyncWebServerRequest *req, String filename, size_t index, uint8_t *data, size_t len,
         bool final) {
        static File ui;
        if (index == 0) {
          if (!storage::ok()) {
            req->send(503, "text/plain", "sem cartao");
            return;
          }
          SD.mkdir(cfg::path::DATA_DIR);
          SD.remove(cfg::path::UI_INDEX);
          ui = SD.open(cfg::path::UI_INDEX, FILE_WRITE);
          if (!ui) {
            req->send(500, "text/plain", "falha ao criar");
            return;
          }
          Serial.println("[UI] recebendo index.html");
        }
        if (ui && len) ui.write(data, len);
        if (final && ui) {
          ui.flush();
          Serial.printf("[UI] instalada (%u bytes)\n", (unsigned)(index + len));
          ui.close();
        }
      });
}
