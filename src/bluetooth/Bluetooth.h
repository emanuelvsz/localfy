#pragma once
// Conexao Bluetooth A2DP com o fone ou a caixa de som.
//
// O ESP32 e a FONTE do audio: ele procura o aparelho, conecta e entrega
// amostras quando o fone pede (ver AudioPipeline). O aparelho procurado e o
// ultimo escolhido no seletor (salvo em Settings); sem escolha, vale o nome
// de secrets.h.
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

// Seletor de aparelhos (tela "Bluetooth" do OLED). beginPicker() desconecta o
// fone atual e lista tudo o que aparecer; pick() conecta no escolhido e o
// salva; cancelPicker() volta a procurar o aparelho salvo.
void beginPicker();
bool pick(size_t index);
void cancelPicker();
bool picking();

void poll();   // chamar no loop: liga o radio e acompanha a conexao

BtState state();
const char *stateName();
bool isConnected();
const String &deviceName();
const String &deviceAddress();
const std::vector<BtDevice> &devices();   // aparelhos vistos na varredura

// Volume: nao passa por aqui. E aplicado nas amostras (AudioPipeline), sem
// comandos de volume pelo radio (ver pipeline::setVolume).
}  // namespace bluetooth
