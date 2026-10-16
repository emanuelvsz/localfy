/* =============================================================================
 * Localfy - tocador de musica autonomo no formato do BMO (ESP32)
 *
 *   cartao SD (MP3/WAV) -> decoder -> Bluetooth A2DP (fone/caixa)
 *   OLED + botoes + knob de volume; modo Web para gerenciar o acervo
 *
 * Este arquivo so liga as pecas na ordem certa. Cada responsabilidade mora
 * na sua pasta (ver src/README.md):
 *   config/     pinos, tempos, limites e dados pessoais (secrets.h)
 *   core/       trava, modo de boot, diagnostico
 *   storage/    cartao SD, biblioteca, metadados, historico
 *   audio/      player e cadeia de decodificacao
 *   bluetooth/  conexao com o fone
 *   net/, web/  Wi-Fi e servidor web (so no modo Web)
 *   ui/, app/   tela, botoes, knob, LED e a logica que os conecta
 * ===========================================================================*/
#include <Arduino.h>
#include <esp_bt.h>
#include <esp_task_wdt.h>

#include "app/App.h"
#include "audio/AudioPipeline.h"
#include "audio/Player.h"
#include "bluetooth/Bluetooth.h"
#include "config/Config.h"
#include "core/BootMode.h"
#include "core/Diagnostics.h"
#include "core/Lock.h"
#include "net/WifiLink.h"
#include "storage/History.h"
#include "storage/Library.h"
#include "storage/MetaStore.h"
#include "storage/Storage.h"
#include "ui/Display.h"
#include "ui/StatusLed.h"
#include "ui/VolumeKnob.h"
#include "web/WebServer.h"

namespace {
// Modo Bluetooth: sem Wi-Fi, toda a RAM restante fica para a pilha Bluetooth.
void startBluetoothMode() {
  wifilink::off();
  // O decoder MP3 e reservado agora, com a RAM ainda folgada: depois que o
  // Bluetooth sobe nao ha mais bloco contiguo para ele.
  Serial.printf("[MP3] decoder %s\n", pipeline::reserveDecoder() ? "alocado" : "FALHOU");
  pipeline::startDecoderTask();
  diag::heapMark("modo BT");
  bluetooth::requestStart();   // o loop liga o radio e conecta no fone
}

// Comandos de diagnostico pelo Monitor Serial (uma palavra por linha):
//   fim   pula para 5 s antes do fim da faixa (testa a troca automatica)
//   prox  proxima faixa      ant  faixa anterior
//   info  faixa, posicao, fila e memoria
void handleSerialCommand() {
  static String line;
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c != '\n' && c != '\r') {
      if (line.length() < 32) line += c;
      continue;
    }
    line.trim();
    if (line.isEmpty()) continue;
    Serial.printf("[CMD] %s\n", line.c_str());
    {
      Guard g;
      if (line == "fim") {
        uint32_t d = player::durationSecs();
        if (d > 6) player::seek(d - 5);
      } else if (line == "prox") {
        player::next();
      } else if (line == "ant") {
        player::previous();
      } else if (line == "info") {
        int i = player::currentIndex();
        Serial.printf("[CMD] faixa %d %s | %lus/%lus | %s | fila %s%s | heap %u\n", i,
                      i >= 0 ? library::nameOf(i).c_str() : "-",
                      (unsigned long)player::positionSecs(), (unsigned long)player::durationSecs(),
                      player::isPlaying() ? "tocando" : "parado",
                      library::filterLabel(player::queueFilter()).c_str(),
                      player::shuffle() ? " aleatorio" : "", (unsigned)ESP.getFreeHeap());
      }
    }
    line = "";
  }
}

// Modo Web: rede propria do Localfy + servidor; o Bluetooth fica desligado.
void startWebMode() {
  wifilink::startAccessPoint();
  diag::heapMark("apos Wi-Fi");
  webserver::begin();
  diag::heapMark("apos servidor");
}
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n=== Localfy ===");
  diag::logResetReason();

  lock::begin();
  esp_task_wdt_init(cfg::sys::WATCHDOG_TIMEOUT_S, true);
  esp_task_wdt_add(NULL);

  Mode mode = bootmode::resolve();
  Serial.printf("[MODO] %s\n", bootmode::label(mode));

  statusled::begin();
  statusled::set(mode == Mode::Web);
  app::begin();
  volumeknob::begin();

  // Nao usamos Bluetooth Low Energy. Devolver essa memoria antes de qualquer
  // outra alocacao deixa mais RAM, e menos fragmentada, para o Bluetooth
  // Classico, que e o que transporta o audio.
  esp_bt_controller_mem_release(ESP_BT_MODE_BLE);
  diag::heapMark("inicio");

  display::begin();
  display::showSplash(mode);
  diag::heapMark("apos OLED");

  if (storage::begin()) {
    library::scan();
    player::loadQueue();
    diag::heapMark("apos biblio");
    meta::load();
    diag::heapMark("apos meta");
    history::ensureHeader();
  }

  if (mode == Mode::Bluetooth) startBluetoothMode();
  else startWebMode();
}

void loop() {
  esp_task_wdt_reset();   // enquanto o laco girar, o cao de guarda nao reinicia
  diag::tick();
  app::update();
  bluetooth::poll();
  wifilink::poll();   // DNS que abre a interface sozinha no celular
  handleSerialCommand();
  player::tick();
  delay(5);
  app::render();
}
