#pragma once
// Metadados editaveis pela interface web (artista e genero de cada musica).
// Ficam na RAM e sao gravados em /localfy/meta.json a cada alteracao (sao
// poucos bytes e mudam raramente).
#include <Arduino.h>

#include <vector>

struct Meta {
  String file;
  String artist;
  String genre;
};

namespace meta {
void load();
const Meta *find(const String &file);
void set(const String &file, const String &artist, const String &genre);
const std::vector<Meta> &all();
// A musica mudou de nome ou de pasta: os metadados vao junto.
void rename(const String &from, const String &to);
}  // namespace meta
