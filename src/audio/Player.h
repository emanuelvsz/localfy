#pragma once
// Player: o que esta tocando, play/pause, troca de faixa, volume e historico.
//
// Funcoes que mudam o estado devem ser chamadas com a trava segura:
//     { Guard g; player::next(); }
// As de leitura (isPlaying, positionSecs...) podem ser chamadas sem trava.
#include <stdint.h>

namespace player {
bool play(int index);      // toca a faixa 'index' da biblioteca
void stop();               // para e esquece a faixa atual
void pause();
void resume();             // retoma, ou comeca pela primeira faixa
void togglePause();
void next();
void previous();           // como no Spotify: volta ao inicio se ja passou de 3 s
void seek(uint32_t secs);

void setVolume(uint8_t percent);
uint8_t volume();

void onLinkLost();         // o fone desconectou
// Chamar no loop (sem a trava): conta o tempo ouvido e passa para a proxima
// faixa quando a atual termina.
void tick();

int currentIndex();        // -1 = nada carregado
bool isPlaying();
uint32_t positionSecs();
uint32_t durationSecs();   // 0 = desconhecida
}  // namespace player
