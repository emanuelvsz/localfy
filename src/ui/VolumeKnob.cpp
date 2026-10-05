#include "ui/VolumeKnob.h"

#include <Arduino.h>

#include "audio/Player.h"
#include "config/Config.h"

namespace {
uint32_t s_lastSampleMs = 0;
float s_smoothed = -1;   // media movel da leitura bruta (0..4095)
int s_applied = -1;      // ultimo volume aplicado (0..100)

// Le, suaviza e aplica. Devolve true se o volume mudou.
bool sample() {
  using namespace cfg::ui;
  int raw = analogRead(cfg::pin::VOLUME_POT);
  s_smoothed = s_smoothed < 0 ? raw : s_smoothed * 0.8f + raw * 0.2f;
  int v = map(constrain((int)s_smoothed, POT_RAW_MIN, POT_RAW_MAX), POT_RAW_MIN, POT_RAW_MAX, 0, 100);
  if (v == s_applied) return false;
  // Faixa morta contra o ruido, mas sempre deixando chegar em 0 e 100.
  if (s_applied >= 0 && abs(v - s_applied) < POT_DEADBAND && v != 0 && v != 100) return false;
  s_applied = v;
  player::setVolume((uint8_t)v);
  return true;
}
}  // namespace

void volumeknob::begin() {
  sample();
  s_lastSampleMs = millis();
  Serial.printf("[VOL] potenciometro GPIO%d -> %u%%\n", cfg::pin::VOLUME_POT,
                (unsigned)player::volume());
}

bool volumeknob::poll() {
  if (millis() - s_lastSampleMs < cfg::ui::POT_SAMPLE_MS) return false;
  s_lastSampleMs = millis();
  return sample();
}
