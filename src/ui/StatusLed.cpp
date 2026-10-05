#include "ui/StatusLed.h"

#include <Arduino.h>

#include "config/Config.h"

void statusled::begin() {
  pinMode(cfg::pin::LED_WEB, OUTPUT);
  set(false);
}

void statusled::set(bool on) { digitalWrite(cfg::pin::LED_WEB, on ? HIGH : LOW); }
