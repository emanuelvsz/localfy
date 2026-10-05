#pragma once
// Cadeia de audio:  arquivo -> decoder MP3 -> buffer PCM -> Bluetooth.
//
// A biblioteca A2DP *puxa* amostras por callback, na task do Bluetooth. Uma
// task propria (o "decoder") le o arquivo, decodifica e enche um buffer
// circular; o callback do Bluetooth so consome desse buffer. Separadas, a
// decodificacao nunca espera a tela nem o servidor web.
//
// Quem decide O QUE tocar e o Player; este modulo so cuida do fluxo de bytes.
// Todas as funcoes, exceto fill(), devem ser chamadas com a trava (Guard).
#include <Arduino.h>

namespace pipeline {

struct OpenResult {
  bool ok = false;
  uint32_t durationSecs = 0;   // estimativa inicial (0 = desconhecida)
  uint32_t sizeBytes = 0;
};

// Reserva o decoder MP3 uma unica vez, cedo no boot: depois que a pilha
// Bluetooth sobe nao sobra bloco contiguo de memoria para ele.
bool reserveDecoder();
void startDecoderTask();
// Cria o buffer PCM. So depois que o enlace Bluetooth fecha: criar antes
// deixava pouca RAM para a inicializacao da pilha Bluetooth.
bool createBuffer();

OpenResult open(const String &path, bool isWav);
void close();                        // fecha o arquivo e descarta o buffer
bool finished();                     // arquivo lido ate o fim e buffer vazio
void seek(uint32_t bytePos, uint32_t secs);

uint32_t filePosition();
uint64_t framesPlayed();             // quadros entregues ao fone (relogio do player)

// Chamado pela task do Bluetooth pedindo 'bytes' de PCM. Entrega o que houver
// e completa com silencio: nunca bloqueia, pois essa task tem prazo rigido.
void fill(uint8_t *dst, size_t bytes);

// Diagnostico: le e zera os contadores do ultimo intervalo.
struct Stats {
  uint32_t gaps;        // pedidos atendidos com silencio (engasgos)
  uint32_t maxStepMs;   // passo de decodificacao mais lento
  uint32_t steps;
  uint32_t stackFree;   // folga da pilha da task do decoder
};
Stats takeStats();
}  // namespace pipeline
