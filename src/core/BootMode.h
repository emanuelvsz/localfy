#pragma once
// Modo de operacao do aparelho.
//
// Wi-Fi + servidor web + Bluetooth Classico nao cabem juntos na RAM deste
// ESP32 (sem PSRAM). Por isso o aparelho funciona em um modo de cada vez:
//   Bluetooth (padrao): toca no fone, controle pelos botoes e pelo OLED.
//   Web:                sem Bluetooth; interface web para gerenciar o acervo.
//
// Desmontar a pilha Bluetooth em tempo de execucao nao devolve a memoria de
// forma confiavel, entao trocar de modo = gravar o pedido e reiniciar.
#include <stdint.h>

enum class Mode : uint8_t { Bluetooth = 0, Web = 1 };

namespace bootmode {
// Le o pedido gravado antes do ultimo reinicio e decide o modo deste boot.
// Ligar o aparelho do zero sempre comeca no Bluetooth.
Mode resolve();
Mode current();
Mode other();
const char *label(Mode mode);
// Grava o pedido e reinicia a placa. Nao retorna.
[[noreturn]] void restartInto(Mode mode);
}  // namespace bootmode
