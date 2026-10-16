#pragma once
// Biblioteca de musicas: os arquivos de audio em /music no cartao.
//
// Pastas de artista: /music/<Artista>/<arquivo>. Uma musica dentro de pasta e
// identificada pelo caminho relativo ("Joji/LOVE YOU LESS.mp3"); as soltas na
// raiz, so pelo nome do arquivo. Um nivel de pasta.
//
// Memoria: todos os nomes (pastas e arquivos) ficam num UNICO bloco de texto,
// cada pasta uma vez so, e cada musica guarda so onde o nome dela comeca. Com
// uma String por musica, 177 faixas em 113 pastas custavam ~13 KB a mais e o
// Bluetooth ficava sem RAM para o audio ao conectar.
#include <Arduino.h>

#include <vector>

struct Song {
  uint16_t file;    // posicao do nome do arquivo no bloco de nomes
  int16_t folder;   // indice da pasta, ou -1 se estiver na raiz
  uint32_t size;    // bytes
};

namespace library {
// Filtros de pasta usados pela fila e pelas telas.
constexpr int ALL = -1;    // todas as musicas
constexpr int ROOT = -2;   // so as soltas na raiz ("Sem pasta")

void scan();                           // rele /music (pastas e musicas, em ordem alfabetica)
const std::vector<Song> &songs();      // ordem: soltas, depois pasta a pasta
size_t count();
String nameOf(size_t i);               // "Pasta/arquivo.mp3" (identifica a musica)
const char *fileOf(size_t i);          // so o arquivo, sem a pasta

size_t folderCount();
const char *folderName(int folder);    // pastas em ordem alfabetica
int findFolder(const String &name);    // indice ou -1
bool hasRootSongs();
bool inFilter(size_t songIndex, int filter);
size_t countIn(int filter);
String filterLabel(int filter);        // "Todas", "Sem pasta" ou o nome da pasta

int find(const String &name);          // indice ou -1
String pathOf(const String &name);     // "/music/<nome>"
String fileName(const String &name);   // so o arquivo, sem a pasta

String extension(const String &name);  // em minusculas, sem o ponto
bool isAudio(const String &name);      // .mp3 ou .wav
// Nome de pasta aceitavel: sem barras, sem ".." e sem comecar com ponto.
bool isSafeFolder(const String &folder);
// "arquivo.mp3" ou "Pasta/arquivo.mp3": no maximo um nivel, sem "../".
bool isSafeName(const String &name);
}  // namespace library
