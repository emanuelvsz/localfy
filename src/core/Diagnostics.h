#pragma once
// Mensagens de diagnostico na serial (115200 baud).
//
// [HEAP]  memoria livre em cada etapa do boot, para localizar consumo.
// [LOOP]  pulso a cada 5 s: se parar de aparecer, o laco principal travou.
// [AUDIO] taxa real de consumo pelo fone (~44100 quadros/s) e engasgos.

namespace diag {
// BROWNOUT (6) indica queda de tensao; PANIC (12) indica excecao no codigo.
// Distinguir os dois evita procurar bug de software em problema eletrico.
void logResetReason();
void heapMark(const char *stage);
void tick();   // chamar no loop; imprime o pulso no intervalo configurado
}  // namespace diag
