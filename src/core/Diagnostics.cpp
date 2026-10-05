#include "core/Diagnostics.h"

#include <Arduino.h>
#include <WiFi.h>

#include "audio/AudioPipeline.h"
#include "audio/Player.h"
#include "bluetooth/Bluetooth.h"
#include "config/Config.h"

namespace {
uint32_t largestFreeBlock() {
  return (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
}

void logAudio() {
  // Taxa real de consumo pelo fone: deveria ser ~44100 quadros/s.
  static uint64_t lastFrames = 0;
  static uint32_t lastMs = 0;
  uint64_t frames = pipeline::framesPlayed();
  uint32_t now = millis();
  // frames < lastFrames quando a faixa troca e o contador zera.
  uint32_t rate = (lastMs && now > lastMs && frames >= lastFrames)
                      ? (uint32_t)((frames - lastFrames) * 1000 / (now - lastMs))
                      : 0;
  lastFrames = frames;
  lastMs = now;

  pipeline::Stats s = pipeline::takeStats();
  Serial.printf("[AUDIO] consumo %u q/s | tocado %lus de %lus | engasgos +%u | "
                "passo max %lu ms (%u passos) | pilha %u\n",
                (unsigned)rate, (unsigned long)player::positionSecs(),
                (unsigned long)player::durationSecs(), (unsigned)s.gaps,
                (unsigned long)s.maxStepMs, (unsigned)s.steps, (unsigned)s.stackFree);
}
}  // namespace

void diag::logResetReason() {
  Serial.printf("[BOOT] motivo do reset: %d (1=power-on 3=sw 6=brownout 12=panic)\n",
                (int)esp_reset_reason());
}

void diag::heapMark(const char *stage) {
  Serial.printf("[HEAP] %-14s livre=%6u maior=%6u\n", stage, (unsigned)ESP.getFreeHeap(),
                (unsigned)largestFreeBlock());
}

void diag::tick() {
  static uint32_t last = 0;
  if (millis() - last < cfg::sys::DIAG_INTERVAL_MS) return;
  last = millis();

  if (player::isPlaying()) logAudio();
  Serial.printf("[LOOP] vivo t=%lus bt=%d wifi=%s heap=%u maior=%u\n",
                (unsigned long)(millis() / 1000), (int)bluetooth::state(),
                WiFi.isConnected() ? "ok" : "--", (unsigned)ESP.getFreeHeap(),
                (unsigned)largestFreeBlock());
}
