#pragma once
// Grupos de rotas da API. Cada um e implementado no arquivo de mesmo nome.
#include <ESPAsyncWebServer.h>

namespace routes {
void registerUi(AsyncWebServer &server);         // RoutesUi.cpp:        / e instalacao da interface
void registerLibrary(AsyncWebServer &server);    // RoutesLibrary.cpp:   musicas, metadados, upload
void registerPlayer(AsyncWebServer &server);     // RoutesPlayer.cpp:    status e controles
void registerStats(AsyncWebServer &server);      // RoutesStats.cpp:     historico e estatisticas
void registerBluetooth(AsyncWebServer &server);  // RoutesBluetooth.cpp: busca e conexao de fones
}  // namespace routes
