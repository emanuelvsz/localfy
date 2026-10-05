#pragma once
// Cartao SD: montagem e estado.
//
// Sem cartao nao da para tocar nada, mas o resto do sistema sobe mesmo assim
// (o modo Web mostra o erro na pagina). ok() gateia tudo que depende do cartao.

namespace storage {
bool begin();   // monta o cartao, tentando frequencias da mais rapida a mais lenta
bool ok();

// Upload em andamento: disputa o barramento SPI com o audio, e a interface
// pode avisar isso ao usuario.
void setUploading(bool active);
bool uploading();
}  // namespace storage
