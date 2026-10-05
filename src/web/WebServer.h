#pragma once
// Servidor web do modo Web: serve a interface (index.html no cartao) e a API
// REST que ela usa. Cada grupo de rotas fica no seu arquivo Routes*.cpp.

namespace webserver {
void begin();
}  // namespace webserver
