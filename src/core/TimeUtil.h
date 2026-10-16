#pragma once
#include <time.h>

namespace timeutil {
// O relogio so e confiavel depois que a interface web informa a hora do
// celular (POST /api/time); antes disso time() devolve uma data perto de 1970.
// Ele sobrevive a troca de modo (reinicio por software), mas nao a desligar.
inline bool synced() { return time(nullptr) > 1600000000; }   // 2020-09-13
}  // namespace timeutil
