#include "ui/Button.h"

#include <Arduino.h>

#include "config/Config.h"

Button::Button(uint8_t pin, uint32_t longPressMs) : pin_(pin), longPressMs_(longPressMs) {}

void Button::begin() { pinMode(pin_, INPUT_PULLUP); }

Button::Event Button::poll() {
  uint32_t now = millis();
  bool down = digitalRead(pin_) == LOW;

  if (down && !pressed_) {
    // Borda de descida. Ignorada logo apos um evento: e o repique do contato.
    if (now - lastEventMs_ < cfg::ui::DEBOUNCE_MS) return Event::None;
    pressed_ = true;
    longFired_ = false;
    downSinceMs_ = now;
    return Event::None;
  }

  if (down && pressed_ && longPressMs_ > 0 && !longFired_ &&
      now - downSinceMs_ >= longPressMs_) {
    longFired_ = true;
    lastEventMs_ = now;
    return Event::LongPress;
  }

  if (!down && pressed_) {
    pressed_ = false;
    if (!longFired_) {
      lastEventMs_ = now;
      return Event::Click;
    }
  }
  return Event::None;
}
