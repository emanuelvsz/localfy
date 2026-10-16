#pragma once
// Wi-Fi (so no modo Web).
//
// O Localfy nao entra em roteador nenhum: no modo Web ele cria a PROPRIA rede
// (nome e senha em Config.h), e o celular ou o computador entra nela para
// abrir a interface em 192.168.4.1. Assim funciona em qualquer lugar.
#include <Arduino.h>

namespace wifilink {
// Desliga o Wi-Fi: no modo Bluetooth toda a RAM fica para a pilha Bluetooth.
void off();
// Cria a rede do Localfy. Falso se o radio nao subir.
bool startAccessPoint();
bool up();
const char *networkName();
const char *password();
String ip();
int clients();   // aparelhos conectados na rede do Localfy
// Chamar no loop: responde o DNS que abre a interface sozinha no celular.
void poll();
}  // namespace wifilink
