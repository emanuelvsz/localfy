#pragma once
// =============================================================================
// Configuracao central do Localfy.
//
// Tudo que depende da montagem fisica (pinos) ou de uma escolha de projeto
// (tempos, limites, caminhos) fica aqui, para nao haver numeros soltos pelo
// codigo. Dados pessoais (Wi-Fi, nome do fone) ficam em secrets.h.
// =============================================================================
#include <stddef.h>
#include <stdint.h>

#include "secrets.h"

namespace cfg {

constexpr const char *DEVICE_NAME = "Localfy";
constexpr const char *WIFI_SSID = LOCALFY_WIFI_SSID;
constexpr const char *WIFI_PASS = LOCALFY_WIFI_PASS;

#ifdef LOCALFY_BT_TARGET
constexpr const char *BT_TARGET = LOCALFY_BT_TARGET;
#else
constexpr const char *BT_TARGET = "JBL WAVE FLEX";
#endif

// -----------------------------------------------------------------------------
// Pinos
// -----------------------------------------------------------------------------
namespace pin {
// Cartao SD no barramento VSPI.
constexpr int SD_CS = 5;
constexpr int SD_SCK = 18;
constexpr int SD_MISO = 19;
constexpr int SD_MOSI = 23;

// OLED SSD1306 (I2C).
constexpr int OLED_SDA = 21;
constexpr int OLED_SCL = 22;

// Botoes: ligados entre o GPIO e o GND, com o pull-up interno (nivel baixo =
// apertado). Escolhidos por medicao: nesta placa os GPIO 2, 4 e 13 ficam
// presos em nivel baixo mesmo com pull-up, e seriam lidos como botao apertado.
constexpr int BTN_NEXT = 32;   // tracinho azul da direita
constexpr int BTN_PREV = 33;   // tracinho azul da esquerda
constexpr int BTN_PLAY = 27;   // botao verde
constexpr int BTN_MODE = 26;   // botao vermelho

// LED branco aceso no modo Web. GPIO15 e pino de boot, mas o LED no maximo
// silencia o log da ROM na serial.
constexpr int LED_WEB = 15;

// Potenciometro de volume (cursor). ADC1: funciona com o Wi-Fi ligado.
constexpr int VOLUME_POT = 35;
}  // namespace pin

// -----------------------------------------------------------------------------
// Display
// -----------------------------------------------------------------------------
namespace oled {
constexpr uint8_t ADDRESS = 0x3C;
constexpr int WIDTH = 128;
constexpr int HEIGHT = 64;
constexpr int LIST_ROWS = 4;            // faixas por pagina na lista
constexpr uint32_t REFRESH_MS = 400;    // ~2,5 fps: suficiente e poupa CPU
}  // namespace oled

// -----------------------------------------------------------------------------
// Interface fisica (botoes, potenciometro, telas)
// -----------------------------------------------------------------------------
namespace ui {
constexpr uint32_t DEBOUNCE_MS = 220;          // contatos mecanicos oscilam ao fechar
constexpr uint32_t LONG_PRESS_MS = 3000;       // segurar "proxima" na lista = tocar
constexpr uint32_t LIBRARY_TIMEOUT_MS = 8000;  // lista volta ao player se ociosa
constexpr uint32_t CONFIRM_TIMEOUT_MS = 10000; // pergunta de troca de modo expira
constexpr uint32_t VOLUME_OVERLAY_MS = 1500;   // "Volume 45%" na barra de cima
constexpr uint32_t POT_SAMPLE_MS = 50;
constexpr int POT_DEADBAND = 2;                // pontos de volume ignorados (ruido do ADC)
// As pontas do ADC sao pouco lineares; as margens garantem 0% e 100%.
constexpr int POT_RAW_MIN = 80;
constexpr int POT_RAW_MAX = 4000;
constexpr uint8_t DEFAULT_VOLUME = 80;
}  // namespace ui

// -----------------------------------------------------------------------------
// Cartao SD
// -----------------------------------------------------------------------------
namespace path {
constexpr const char *MUSIC_DIR = "/music";
constexpr const char *DATA_DIR = "/localfy";
constexpr const char *UI_INDEX = "/localfy/index.html";
constexpr const char *META = "/localfy/meta.json";
constexpr const char *META_TMP = "/localfy/meta.tmp";
constexpr const char *HISTORY = "/localfy/history.csv";
}  // namespace path

namespace sd {
// Frequencias tentadas em ordem. A 4 MHz (padrao da biblioteca) cada leitura
// de 1 KB de MP3 podia levar dezenas de ms e o audio nao acompanhava.
constexpr uint32_t SD_FREQS_HZ[] = {20000000, 10000000, 4000000};
// O player abre no maximo o audio e o historico ao mesmo tempo. O padrao (5)
// reserva buffers que fazem falta ao Bluetooth.
constexpr uint8_t SD_MAX_FILES = 2;
constexpr size_t MAX_SONGS = 256;
constexpr size_t MAX_NAME_LEN = 64;
}  // namespace sd

// -----------------------------------------------------------------------------
// Audio
// -----------------------------------------------------------------------------
namespace audio {
constexpr uint32_t SAMPLE_RATE = 44100;    // taxa fixa do A2DP/SBC
// Buffer PCM entre o decoder e o Bluetooth: 8 KB (~46 ms). Menor nao cabe a
// saida de um quadro MP3 (4608 B) com folga; maior faltava RAM depois que o
// enlace Bluetooth fecha.
constexpr size_t PCM_BUFFER_BYTES = 8 * 1024;
// Decodifica sempre que houver pelo menos isto de espaco no buffer.
constexpr size_t PCM_HEADROOM = 1024;
// Tarefa do decoder: core 1, prioridade acima do loop. Pilha de 3,5 KB: com
// 6 KB o maior bloco livre caia a 4 KB e o Bluetooth deixava de alocar pacotes.
constexpr uint32_t DECODER_STACK = 3584;
constexpr int DECODER_PRIORITY = 3;
constexpr int DECODER_CORE = 1;
}  // namespace audio

// Uma faixa conta no historico depois de 20 s ou de 35% da duracao.
namespace scrobble {
constexpr uint32_t MIN_SECS = 20;
constexpr float MIN_RATIO = 0.35f;
}  // namespace scrobble

// -----------------------------------------------------------------------------
// Bluetooth, Wi-Fi e relogio
// -----------------------------------------------------------------------------
namespace bt {
constexpr size_t MAX_DEVICES = 20;
}  // namespace bt

namespace net {
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 10000;
constexpr long GMT_OFFSET_SEC = -3 * 3600;   // Brasil, sem horario de verao
constexpr int DST_OFFSET_SEC = 0;
constexpr const char *NTP_SERVER_1 = "pool.ntp.org";
constexpr const char *NTP_SERVER_2 = "a.st1.ntp.br";
}  // namespace net

namespace sys {
// Cao de guarda: a biblioteca A2DP pode bloquear a task do Arduino durante a
// descoberta. 45 s cobrem a conexao normal com folga.
constexpr uint32_t WATCHDOG_TIMEOUT_S = 45;
constexpr uint32_t DIAG_INTERVAL_MS = 5000;
}  // namespace sys

}  // namespace cfg
