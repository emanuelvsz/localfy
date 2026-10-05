#pragma once
// Estado da interface do aparelho: qual tela esta aberta e o que ela mostra.
// Quem muda este estado e o App (reagindo aos botoes); o Display so desenha.
#include <stdint.h>

enum class Screen : uint8_t {
  Player,        // o que esta tocando (padrao no modo Bluetooth)
  Library,       // lista de musicas para escolher
  ConfirmMode,   // "Trocar de modo?" depois do botao vermelho
  WebHome,       // tela do modo Web: endereco para abrir no navegador
};

struct UiState {
  Screen screen = Screen::Player;
  int cursor = 0;               // faixa destacada na lista
  uint32_t volumeShownAt = 0;   // quando o knob mudou o volume (0 = nunca)
};
