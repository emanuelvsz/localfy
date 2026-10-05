#pragma once
// Display OLED SSD1306 (128x64, monocromatico).
#include "core/BootMode.h"
#include "ui/UiState.h"

namespace display {
bool begin();                       // falso se nao houver display no barramento
void showSplash(Mode mode);         // tela de boot
void showModeChange(Mode target);   // "Modo Web / reiniciando..." (desenha na hora)
void render(const UiState &ui);     // tela normal, limitada a ~2,5 fps
}  // namespace display
