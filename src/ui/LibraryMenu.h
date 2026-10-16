#pragma once
// Itens da lista de musicas do aparelho, compartilhados pela logica (App) e
// pelo desenho (Display).
//
// Nivel 0 (pastas):  0 = "Aleatorio: sim/nao", 1 = "Todas", depois "Sem
//                    pasta" (se houver musicas soltas e pastas) e cada pasta.
// Nivel 1 (musicas): as faixas do filtro escolhido, em ordem alfabetica.
#include "storage/Library.h"

namespace libmenu {
inline bool showRoot() { return library::hasRootSongs() && library::folderCount() > 0; }

inline int folderItems() { return 2 + (showRoot() ? 1 : 0) + (int)library::folderCount(); }

// Filtro do item 'i' do nivel 0 (i >= 1).
inline int filterAt(int i) {
  if (i <= 1) return library::ALL;
  if (showRoot()) {
    if (i == 2) return library::ROOT;
    return i - 3;
  }
  return i - 2;
}

// Item do nivel 0 que corresponde a um filtro.
inline int itemOf(int filter) {
  for (int i = 1; i < folderItems(); i++)
    if (filterAt(i) == filter) return i;
  return 1;
}

// Indice (na biblioteca) da 'pos'-esima musica do filtro, ou -1.
inline int songAt(int filter, int pos) {
  for (size_t i = 0; i < library::count(); i++)
    if (library::inFilter(i, filter) && pos-- == 0) return (int)i;
  return -1;
}

// Posicao de uma musica dentro do filtro, ou -1.
inline int posOf(int filter, int songIndex) {
  int pos = 0;
  for (size_t i = 0; i < library::count(); i++) {
    if (!library::inFilter(i, filter)) continue;
    if ((int)i == songIndex) return pos;
    pos++;
  }
  return -1;
}
}  // namespace libmenu
