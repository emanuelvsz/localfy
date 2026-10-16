#pragma once
// Botao fisico com debounce, ligado entre o GPIO e o GND (pull-up interno).
//
// Contatos mecanicos oscilam por alguns milissegundos ao fechar; sem o
// debounce um toque contaria varias vezes.
#include <stdint.h>

class Button {
 public:
  enum class Event : uint8_t { None, Click, LongPress };

  // longPressMs = 0 desliga a pressao longa (so existe o clique).
  // repeatMs > 0: continuando apertado, repete o LongPress nesse intervalo.
  explicit Button(uint8_t pin, uint32_t longPressMs = 0, uint32_t repeatMs = 0);
  void begin();
  // Chamar no loop. Click sai ao soltar; LongPress sai ainda apertado,
  // dando retorno imediato, e nesse caso o soltar nao gera Click.
  Event poll();

 private:
  uint8_t pin_;
  uint32_t longPressMs_;
  uint32_t repeatMs_;
  bool pressed_ = false;
  bool longFired_ = false;
  uint32_t downSinceMs_ = 0;
  uint32_t lastEventMs_ = 0;
};
