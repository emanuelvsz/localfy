#pragma once
// Leitura do inicio de uma faixa: onde o audio comeca e quanto ele dura.
//
// MP3 baixado costuma trazer uma etiqueta ID3 no inicio, com a capa do album
// (JPEG de dezenas a centenas de KB). O audio so comeca depois dela, e a capa
// nao pode ir para o decoder nem entrar na conta da duracao.
#include <FS.h>

namespace duration {
struct Info {
  uint32_t audioStart = 0;   // byte onde o audio comeca (depois de cabecalho/etiqueta)
  uint32_t secs = 0;         // duracao em segundos (0 = desconhecida)
  bool exact = false;        // veio do cabecalho (WAV, Xing/Info/VBRI), nao de estimativa
  uint32_t sampleRate = 44100;
  uint8_t channels = 2;
};

Info ofWav(File &f);
// Pula a etiqueta ID3, acha o primeiro quadro valido (confirmado pelo
// seguinte) e calcula a duracao: exata pelo cabecalho Xing/Info/VBRI quando
// existe (MP3 de taxa variavel), senao pela taxa de bits do quadro.
Info ofMp3(File &f);
}  // namespace duration
