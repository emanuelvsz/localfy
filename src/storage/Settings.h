#pragma once
// Escolhas do usuario que precisam sobreviver ao desligamento: o fone/caixa
// Bluetooth e a fila (pasta + aleatorio). Ficam na memoria interna do ESP32 (NVS), e nao no
// cartao: assim funcionam mesmo sem cartao e nao somem ao trocar de cartao.
//
// Sem nada salvo, valem os padroes de config/secrets.h.
#include <Arduino.h>

namespace settings {
String btName();      // nome do ultimo aparelho escolhido ("" = nenhum)
String btAddress();   // MAC do ultimo aparelho escolhido ("" = nenhum)
void setBt(const String &name, const String &address);

// Fila: pasta pelo NOME ("" = todas, "/" = sem pasta), para sobreviver a
// pastas novas que mudam os indices.
String queueFolder();
bool queueShuffle();
void setQueue(const String &folder, bool shuffle);
}  // namespace settings
