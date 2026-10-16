#pragma once
// Estado da interface do aparelho: qual tela esta aberta e o que ela mostra.
// Quem muda este estado e o App (reagindo aos botoes); o Display so desenha.
#include <stdint.h>

enum class Screen : uint8_t {
  Player,        // o que esta tocando (padrao no modo Bluetooth)
  Library,       // lista de musicas para escolher
  ConfirmMode,   // "Trocar de modo?" depois do botao vermelho
  WebHome,       // tela do modo Web: endereco para abrir no navegador
  Standby,       // cara do BMO; o verde acorda
  BtPicker,      // lista de fones/caixas para escolher (segurar o vermelho)
};

struct UiState {
  Screen screen = Screen::Player;
  int cursor = 0;               // item destacado na lista
  uint8_t libLevel = 1;         // lista: 0 = pastas, 1 = musicas da pasta
  int libFilter = -1;           // pasta aberta no nivel 1 (library::ALL, ROOT ou indice)
  uint32_t volumeShownAt = 0;   // quando o knob mudou o volume (0 = nunca)
};
