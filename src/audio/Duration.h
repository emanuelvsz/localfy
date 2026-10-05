#pragma once
// Estimativa de duracao de uma faixa, em segundos (0 = desconhecida).
//
// WAV: le o cabecalho. MP3: usa o bitrate do primeiro quadro valido, o que
// erra em arquivos de taxa variavel (VBR). Por isso o player refina a duracao
// enquanto toca (ver Player.cpp) e a interface trata duracao 0.
#include <FS.h>

namespace duration {
uint32_t ofWav(File &f);
uint32_t ofMp3(File &f);
}  // namespace duration
