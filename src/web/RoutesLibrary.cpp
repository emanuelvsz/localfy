// Rotas da biblioteca: listar, editar metadados, enviar e apagar musicas.
#include <SD.h>

#include "audio/Player.h"
#include "core/Lock.h"
#include "storage/Library.h"
#include "storage/MetaStore.h"
#include "storage/Storage.h"
#include "web/Http.h"
#include "web/Routes.h"

void routes::registerLibrary(AsyncWebServer &server) {
  server.on("/api/songs", HTTP_GET, [](AsyncWebServerRequest *req) {
    if (!storage::ok()) {
      http::sendError(req, 503, "sem cartao SD");
      return;
    }
    JsonDocument doc;
    JsonArray arr = doc["songs"].to<JsonArray>();
    for (auto &s : library::songs()) {
      JsonObject o = arr.add<JsonObject>();
      o["name"] = s.name;
      o["size"] = s.size;
      const Meta *m = meta::find(s.name);
      o["artist"] = m ? m->artist : "";
      o["genre"] = m ? m->genre : "";
    }
    http::sendJson(req, 200, doc);
  });

  http::onJsonPost(server, "/api/meta", [](AsyncWebServerRequest *req, JsonDocument &doc) {
    String file = doc["file"] | "";
    if (!library::isSafeName(file)) {
      http::sendError(req, 400, "nome invalido");
      return;
    }
    Guard g;
    if (library::find(file) < 0) {
      http::sendError(req, 404, "musica inexistente");
      return;
    }
    String artist = doc["artist"] | "";
    String genre = doc["genre"] | "";
    meta::set(file, artist.substring(0, 64), genre.substring(0, 32));
    http::sendOk(req);
  });

  // Generos ja usados, para a interface sugerir no autocompletar.
  server.on("/api/genres", HTTP_GET, [](AsyncWebServerRequest *req) {
    JsonDocument doc;
    JsonArray arr = doc["genres"].to<JsonArray>();
    for (auto &m : meta::all()) {
      if (m.genre.isEmpty()) continue;
      bool dup = false;
      for (JsonVariant v : arr)
        if (m.genre == v.as<const char *>()) {
          dup = true;
          break;
        }
      if (!dup) arr.add(m.genre);
    }
    http::sendJson(req, 200, doc);
  });

  http::onJsonPost(server, "/api/delete", [](AsyncWebServerRequest *req, JsonDocument &doc) {
    String file = doc["file"] | "";
    if (!library::isSafeName(file)) {
      http::sendError(req, 400, "nome invalido");
      return;
    }
    Guard g;
    int idx = library::find(file);
    if (idx < 0) {
      http::sendError(req, 404, "musica inexistente");
      return;
    }
    if (idx == player::currentIndex()) player::stop();   // libera o arquivo antes de apagar
    if (!SD.remove(library::pathOf(file))) {
      http::sendError(req, 500, "falha ao remover");
      return;
    }
    library::scan();
    http::sendOk(req);
  });

  // Upload: os pedacos do multipart sao gravados direto no cartao. Nada de
  // guardar o arquivo na RAM, que nao caberia.
  server.on(
      "/api/upload", HTTP_POST,
      [](AsyncWebServerRequest *req) {
        storage::setUploading(false);
        library::scan();
        http::sendOk(req);
      },
      [](AsyncWebServerRequest *req, String filename, size_t index, uint8_t *data, size_t len,
         bool final) {
        static File up;
        if (index == 0) {
          if (!library::isSafeName(filename)) {
            req->send(400, "text/plain", "nome invalido");
            return;
          }
          storage::setUploading(true);
          String path = library::pathOf(filename);
          SD.remove(path);
          up = SD.open(path, FILE_WRITE);
          if (!up) {
            storage::setUploading(false);
            req->send(500, "text/plain", "falha ao criar");
            return;
          }
          Serial.printf("[UP] %s\n", filename.c_str());
        }
        if (up && len) up.write(data, len);
        if (final) {
          if (up) {
            up.flush();
            up.close();
          }
          storage::setUploading(false);
        }
      });
}
