#pragma once
#include <time.h>

namespace timeutil {
// O relogio so e confiavel depois do NTP (modo Web); antes disso time()
// devolve uma data perto de 1970.
inline bool synced() { return time(nullptr) > 1600000000; }   // 2020-09-13
}  // namespace timeutil
