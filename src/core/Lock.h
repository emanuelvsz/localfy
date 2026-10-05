#pragma once
// Trava global do sistema.
//
// O servidor web roda em outra task (AsyncTCP) e o decoder roda na sua propria
// task. Qualquer acesso ao cartao SD ou ao estado do player precisa passar por
// esta trava, senao duas tasks mexem no mesmo arquivo ao mesmo tempo.
//
// Uso:  { Guard g; player::next(); }   // trava ate o fim do bloco
// A trava NAO e recursiva: nunca crie um Guard dentro de outro.
#include <stdint.h>

namespace lock {
void begin();
bool tryTake(uint32_t timeoutMs);
void give();
}  // namespace lock

struct Guard {
  Guard();
  ~Guard();
  Guard(const Guard &) = delete;
  Guard &operator=(const Guard &) = delete;
};
