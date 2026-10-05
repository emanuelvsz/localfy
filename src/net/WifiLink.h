#pragma once
// Wi-Fi (so no modo Web).
#include <Arduino.h>

namespace wifilink {
// Desliga o Wi-Fi: no modo Bluetooth toda a RAM fica para a pilha Bluetooth.
void off();
// Conecta na rede de secrets.h e acerta o relogio por NTP. Falso se nao
// conectar no tempo limite (o aparelho segue offline).
bool connect();
bool connected();
String ip();
}  // namespace wifilink
