// Rotas da biblioteca: listar, editar metadados, enviar, apagar e organizar
// musicas em pastas de artista; e a fila do aparelho (pasta + aleatorio).
#include <SD.h>

#include "audio/Player.h"
#include "core/Lock.h"
#include "config/Config.h"
#include "storage/Library.h"
#include "storage/MetaStore.h"
#include "storage/Storage.h"
#include "web/Http.h"
#include "web/Routes.h"

namespace {
// Releu o cartao: o player reencontra a faixa atual e refaz a fila.
void rescan() {
  library::scan();
  player::onLibraryChanged();
}

int filterFromName(const String &folder) {
  if (folder.isEmpty()) return library::ALL;
  if (folder == "/") return library::ROOT;
  return library::findFolder(folder);   // -1 (= ALL) se nao existir
}

String filterName(int filter) {
  if (filter == library::ALL) return "";
  if (filter == library::ROOT) return "/";
  return library::filterLabel(filter);
}
}  // namespace

void routes::registerLibrary(AsyncWebServer &server) {
  server.on("/api/songs", HTTP_GET, [](AsyncWebServerRequest *req) {
    if (!storage::ok()) {
      http::sendError(req, 503, "sem cartao SD");
      return;
    }
    JsonDocument doc;
    JsonArray arr = doc["songs"].to<JsonArray>();
    for (size_t i = 0; i < library::count(); i++) {
      const Song &s = library::songs()[i];
      String name = library::nameOf(i);
      JsonObject o = arr.add<JsonObject>();
      o["name"] = name;
      o["size"] = s.size;
      o["folder"] = library::folderName(s.folder);
      const Meta *m = meta::find(name);
      o["artist"] = m ? m->artist : "";
      o["genre"] = m ? m->genre : "";
    }
    JsonArray fs = doc["folders"].to<JsonArray>();
    for (size_t f = 0; f < library::folderCount(); f++) fs.add(library::folderName(f));
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
    rescan();
    http::sendOk(req);
  });

  // Cria uma pasta de artista vazia.
  http::onJsonPost(server, "/api/folder", [](AsyncWebServerRequest *req, JsonDocument &doc) {
    String name = doc["name"] | "";
    name.trim();
    if (!library::isSafeFolder(name)) {
      http::sendError(req, 400, "nome de pasta invalido");
      return;
    }
    Guard g;
    SD.mkdir(String(cfg::path::MUSIC_DIR) + "/" + name);
    rescan();
    http::sendOk(req);
  });

  // Move uma musica para outra pasta ("" = raiz). Os metadados vao junto.
  http::onJsonPost(server, "/api/move", [](AsyncWebServerRequest *req, JsonDocument &doc) {
    String file = doc["file"] | "";
    String folder = doc["folder"] | "";
    folder.trim();
    if (!library::isSafeName(file) || (!folder.isEmpty() && !library::isSafeFolder(folder))) {
      http::sendError(req, 400, "nome invalido");
      return;
    }
    String to = folder.isEmpty() ? library::fileName(file) : folder + "/" + library::fileName(file);
    if (to == file) {
      http::sendOk(req);
      return;
    }
    Guard g;
    int idx = library::find(file);
    if (idx < 0) {
      http::sendError(req, 404, "musica inexistente");
      return;
    }
    if (library::find(to) >= 0) {
      http::sendError(req, 409, "ja existe uma musica com esse nome na pasta");
      return;
    }
    if (idx == player::currentIndex()) player::stop();   // libera o arquivo
    if (!folder.isEmpty()) SD.mkdir(String(cfg::path::MUSIC_DIR) + "/" + folder);
    if (!SD.rename(library::pathOf(file), library::pathOf(to))) {
      http::sendError(req, 500, "falha ao mover");
      return;
    }
    meta::rename(file, to);
    rescan();
    http::sendOk(req);
  });

  // Fila do aparelho: de qual pasta vem a proxima faixa e se e aleatoria.
  // Fica salva e vale no modo Bluetooth (no modo Web nao ha audio).
  server.on("/api/queue", HTTP_GET, [](AsyncWebServerRequest *req) {
    JsonDocument doc;
    doc["folder"] = filterName(player::queueFilter());
    doc["shuffle"] = player::shuffle();
    http::sendJson(req, 200, doc);
  });

  http::onJsonPost(server, "/api/queue", [](AsyncWebServerRequest *req, JsonDocument &doc) {
    String folder = doc["folder"] | "";
    bool shuffle = doc["shuffle"] | false;
    int filter = filterFromName(folder);
    if (!folder.isEmpty() && folder != "/" && filter < 0) {
      http::sendError(req, 404, "pasta inexistente");
      return;
    }
    Guard g;
    player::setQueue(filter, shuffle);
    http::sendOk(req);
  });

  // Upload: os pedacos do multipart sao gravados direto no cartao. Nada de
  // guardar o arquivo na RAM, que nao caberia.
  server.on(
      "/api/upload", HTTP_POST,
      [](AsyncWebServerRequest *req) {
        storage::setUploading(false);
        Guard g;
        rescan();
        http::sendOk(req);
      },
      [](AsyncWebServerRequest *req, String filename, size_t index, uint8_t *data, size_t len,
         bool final) {
        static File up;
        if (index == 0) {
          // Pasta de destino opcional: /api/upload?folder=Sabrina%20Carpenter
          String folder = req->hasParam("folder") ? req->getParam("folder")->value() : String("");
          folder.trim();
          String name = folder.isEmpty() ? filename : folder + "/" + filename;
          if (!library::isSafeName(name)) {
            req->send(400, "text/plain", "nome invalido");
            return;
          }
          storage::setUploading(true);
          if (!folder.isEmpty()) SD.mkdir(String(cfg::path::MUSIC_DIR) + "/" + folder);
          String path = library::pathOf(name);
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
