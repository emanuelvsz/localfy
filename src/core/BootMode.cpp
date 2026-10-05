#include "core/BootMode.h"

#include <Arduino.h>
#include <esp_attr.h>

namespace {
// A memoria RTC_NOINIT sobrevive ao ESP.restart() mas nao a falta de energia.
// O numero magico distingue um pedido real do lixo que ela tem no power-on.
constexpr uint32_t REQUEST_MAGIC = 0x4C4F4346;  // "LOCF"
RTC_NOINIT_ATTR uint32_t s_requestMagic;
RTC_NOINIT_ATTR uint32_t s_requestedMode;

Mode s_mode = Mode::Bluetooth;
}  // namespace

Mode bootmode::resolve() {
  bool wantsWeb = s_requestMagic == REQUEST_MAGIC &&
                  s_requestedMode == static_cast<uint32_t>(Mode::Web);
#ifdef LOCALFY_FORCE_WEB_MODE
  wantsWeb = true;   // ambiente esp32dev-web (platformio.ini)
#endif
  // Consome o pedido: se o modo Web travar e o watchdog reiniciar, volta ao padrao.
  s_requestMagic = 0;
  s_mode = wantsWeb ? Mode::Web : Mode::Bluetooth;
  return s_mode;
}

Mode bootmode::current() { return s_mode; }

Mode bootmode::other() {
  return s_mode == Mode::Bluetooth ? Mode::Web : Mode::Bluetooth;
}

const char *bootmode::label(Mode mode) {
  return mode == Mode::Web ? "Web" : "Bluetooth";
}

void bootmode::restartInto(Mode mode) {
  Serial.printf("[MODO] reiniciando no modo %s\n", label(mode));
  s_requestedMode = static_cast<uint32_t>(mode);
  s_requestMagic = REQUEST_MAGIC;
  delay(300);   // deixa a serial e o display terminarem
  ESP.restart();
  for (;;) {}
}
