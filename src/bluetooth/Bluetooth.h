#pragma once
// Conexao Bluetooth A2DP com o fone ou a caixa de som.
//
// O ESP32 e a FONTE do audio: ele procura o aparelho pelo nome (secrets.h),
// conecta e entrega amostras quando o fone pede (ver AudioPipeline).
#include <Arduino.h>

#include <vector>

enum class BtState : uint8_t { Disconnected, Scanning, Connecting, Connected };

struct BtDevice {
  String name;
  String address;
  int rssi;
};

namespace bluetooth {
// Pedidos. O radio so e ligado dentro de poll(), no loop principal: a task
// do servidor web tem pilha pequena demais para inicializar o Bluetooth.
void requestStart();                                   // conecta no alvo padrao
void requestConnect(const String &name, const String &address);
void requestScan(uint32_t seconds);
void disconnect();

void poll();   // chamar no loop: liga o radio e acompanha a conexao

BtState state();
const char *stateName();
bool isConnected();
const String &deviceName();
const String &deviceAddress();
const std::vector<BtDevice> &devices();   // aparelhos vistos na varredura

// Aplica o volume (0..100) no enlace, se o radio estiver ligado.
void applyVolume(uint8_t percent);
}  // namespace bluetooth
