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

// Modo Web: Wi-Fi + servidor; o Bluetooth fica desligado.
void startWebMode() {
  wifilink::connect();
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
  player::tick();
  delay(5);
  app::render();
}
