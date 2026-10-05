#pragma once
// Biblioteca de musicas: a lista de arquivos de audio em /music no cartao.
#include <Arduino.h>

#include <vector>

struct Song {
  String name;     // nome do arquivo, sem a pasta
  uint32_t size;   // bytes
};

namespace library {
void scan();                           // rele /music (ordem alfabetica)
const std::vector<Song> &songs();
size_t count();
int find(const String &name);          // indice ou -1
String pathOf(const String &name);     // "/music/<nome>"

String extension(const String &name);  // em minusculas, sem o ponto
bool isAudio(const String &name);      // .mp3 ou .wav
// Nome simples e de audio: rejeita "../" e barras, que a interface nunca manda.
bool isSafeName(const String &name);
}  // namespace library
