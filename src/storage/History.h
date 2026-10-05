#pragma once
// Historico de reproducao (/localfy/history.csv).
//
// Append-only: uma linha por musica ouvida, nunca reescreve o arquivo. Uma
// queda de energia custa no maximo a ultima linha.
// Colunas: timestamp,datetime,file,artist,genre,listened_secs,duration_secs,completed
#include <Arduino.h>

namespace history {
constexpr int COLUMNS = 8;
enum Column { TIMESTAMP = 0, DATETIME, FILE_NAME, ARTIST, GENRE, LISTENED, DURATION, COMPLETED };

void ensureHeader();
void append(const String &file, uint32_t listenedSecs, uint32_t durationSecs);
// Separa uma linha do CSV nas colunas (respeitando aspas). Falso se vazia.
bool parseLine(const String &line, String (&fields)[COLUMNS]);
}  // namespace history
