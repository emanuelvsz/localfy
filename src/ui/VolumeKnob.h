#pragma once
// Potenciometro de volume.
//
// O volume so muda quando o knob gira de verdade: o ADC do ESP32 oscila
// alguns pontos mesmo parado. Assim o volume escolhido pela interface web
// tambem vale ate alguem girar o knob.

namespace volumeknob {
void begin();   // le a posicao atual e aplica como volume inicial
bool poll();    // chamar no loop; true quando o knob mudou o volume
}  // namespace volumeknob
