#include "ui/Display.h"

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>

#include "audio/Player.h"
#include "bluetooth/Bluetooth.h"
#include "config/Config.h"
#include "net/WifiLink.h"
#include "storage/Library.h"
#include "storage/MetaStore.h"
#include "storage/Storage.h"

namespace {
using cfg::oled::WIDTH;
constexpr int CHAR_W = 6;                 // largura de um caractere no tamanho 1
constexpr int LINE_CHARS = WIDTH / CHAR_W;   // 21 caracteres por linha

Adafruit_SSD1306 s_oled(cfg::oled::WIDTH, cfg::oled::HEIGHT, &Wire, -1);
bool s_ok = false;

// ---------------------------------------------------------------------------
// Utilidades de texto
// ---------------------------------------------------------------------------
// Encurta o nome do arquivo: tira a extensao, o sufixo de origem ("_site.org")
// e corta com ponto final se ainda nao couber.
String shortTitle(const String &file, int maxChars) {
  String t = file;
  int dot = t.lastIndexOf('.');
  if (dot > 0) t = t.substring(0, dot);
  int suffix = t.indexOf('_');
  if (suffix > 3) t = t.substring(0, suffix);
  t.trim();
  if ((int)t.length() > maxChars) t = t.substring(0, maxChars - 1) + ".";
  return t;
}

String formatTime(uint32_t secs) {
  char buf[12];
  snprintf(buf, sizeof(buf), "%lu:%02lu", (unsigned long)secs / 60, (unsigned long)secs % 60);
  return String(buf);
}

void printAt(int x, int y, const String &text, uint8_t size = 1) {
  s_oled.setTextSize(size);
  s_oled.setCursor(x, y);
  s_oled.print(text);
}

void printRight(int y, const String &text) {
  printAt(WIDTH - (int)text.length() * CHAR_W, y, text);
}

void printCentered(int y, const String &text, uint8_t size = 1) {
  int w = (int)text.length() * CHAR_W * size;
  printAt((WIDTH - w) / 2, y, text, size);
}

// ---------------------------------------------------------------------------
// Barra de status (linha de cima, comum a todas as telas)
// ---------------------------------------------------------------------------
void drawStatusBar(const UiState &ui) {
  bool web = bootmode::current() == Mode::Web;
  String left;
  bool showModeTag = true;

  if (ui.volumeShownAt && millis() - ui.volumeShownAt < cfg::ui::VOLUME_OVERLAY_MS) {
    left = "Volume " + String(player::volume()) + "%";
  } else if (ui.screen == Screen::ConfirmMode) {
    left = player::isPlaying() ? "A musica vai parar" : "TROCAR DE MODO";
    showModeTag = false;
  } else if (ui.screen == Screen::Library) {
    left = "BIBLIOTECA";
  } else if (web) {
    left = wifilink::connected() ? "Wi-Fi conectado" : "sem Wi-Fi";
  } else {
    left = bluetooth::isConnected() ? "BT conectado" : "BT buscando...";
  }

  printAt(0, 0, left);
  if (showModeTag) printRight(0, web ? "WEB" : "BT");
  s_oled.drawFastHLine(0, 10, WIDTH, SSD1306_WHITE);
}

// ---------------------------------------------------------------------------
// Telas
// ---------------------------------------------------------------------------
// O que esta tocando: titulo, artista, tempo e barra de progresso.
void drawPlayer() {
  int idx = player::currentIndex();
  if (idx < 0) {
    printAt(0, 28, storage::ok() ? "Nada tocando" : "Sem cartao SD");
    if (storage::ok() && library::count() > 0) {
      printAt(0, 44, String(library::count()) + " faixas");
      printAt(0, 54, "azul=lista verde=toca");
    }
    return;
  }

  const String &file = library::songs()[idx].name;
  String title = shortTitle(file, LINE_CHARS * 2);
  printAt(0, 15, title.substring(0, LINE_CHARS));
  if ((int)title.length() > LINE_CHARS) printAt(0, 25, title.substring(LINE_CHARS));

  const Meta *m = meta::find(file);
  if (m && !m->artist.isEmpty()) printAt(0, 37, m->artist.substring(0, LINE_CHARS));

  // A duracao de MP3 VBR nao e confiavel: so mostra o total quando existe.
  uint32_t pos = player::positionSecs();
  uint32_t dur = player::durationSecs();
  printAt(0, 50, player::isPlaying() ? ">" : "||");
  printAt(14, 50, dur > 0 ? formatTime(pos) + "/" + formatTime(dur) : formatTime(pos));
  printRight(50, String(idx + 1) + "/" + String(library::count()));
  if (dur > 0) s_oled.drawFastHLine(0, 62, (int)((uint64_t)pos * WIDTH / dur), SSD1306_WHITE);
}

// Lista paginada com a faixa destacada em barra invertida.
void drawLibrary(const UiState &ui) {
  int total = (int)library::count();
  if (total == 0) {
    printAt(0, 28, "Biblioteca vazia");
    return;
  }
  const int rows = cfg::oled::LIST_ROWS;
  int page = ui.cursor / rows;
  int pages = (total + rows - 1) / rows;

  for (int i = 0; i < rows; i++) {
    int idx = page * rows + i;
    if (idx >= total) break;
    int y = 14 + i * 11;
    bool selected = idx == ui.cursor;
    if (selected) s_oled.fillRect(0, y - 2, WIDTH, 11, SSD1306_WHITE);
    s_oled.setTextColor(selected ? SSD1306_BLACK : SSD1306_WHITE);
    printAt(2, y, shortTitle(library::songs()[idx].name, 20));
    if (idx == player::currentIndex()) printAt(WIDTH - 7, y, ">");   // tocando agora
  }
  s_oled.setTextColor(SSD1306_WHITE);

  s_oled.drawFastHLine(0, 57, WIDTH, SSD1306_WHITE);
  printAt(0, 59, String(ui.cursor + 1) + "/" + String(total));
  printRight(59, "pag " + String(page + 1) + "/" + String(pages));
}

// Pergunta aberta pelo botao vermelho.
void drawConfirmMode() {
  Mode target = bootmode::other();
  printAt(0, 14, "Ir para o modo:");
  printCentered(25, target == Mode::Web ? "WEB" : "BLUETOOTH", 2);
  printAt(0, 45, "Verde:    confirmar");
  printAt(0, 55, "Vermelho: cancelar");
}

// Tela do modo Web: onde abrir a interface no navegador.
void drawWebHome() {
  printCentered(15, "Modo Web", 2);
  if (wifilink::connected()) {
    printAt(0, 35, "Abra no navegador:");
    printAt(0, 45, wifilink::ip());
  } else {
    printAt(0, 35, "Wi-Fi nao conectou.");
    printAt(0, 45, "Confira secrets.h");
  }
  printAt(0, 56, "Vermelho: Bluetooth");
}
}  // namespace

bool display::begin() {
  Wire.begin(cfg::pin::OLED_SDA, cfg::pin::OLED_SCL);
  // 400 kHz: o SSD1306 suporta, e cada redesenho cai de ~90 para ~25 ms.
  Wire.setClock(400000);
  // Sonda o endereco antes: begin() pode travar quando nao ha display, e ai o
  // aparelho nunca chegaria a subir o resto.
  Wire.beginTransmission(cfg::oled::ADDRESS);
  bool present = Wire.endTransmission() == 0;
  if (!present) {
    Serial.println("[OLED] nao detectado no barramento I2C");
    return false;
  }
  s_ok = s_oled.begin(SSD1306_SWITCHCAPVCC, cfg::oled::ADDRESS);
  if (!s_ok) Serial.println("[OLED] detectado mas begin() falhou");
  s_oled.setTextColor(SSD1306_WHITE);
  return s_ok;
}

void display::showSplash(Mode mode) {
  if (!s_ok) return;
  s_oled.clearDisplay();
  printAt(0, 20, "Localfy iniciando");
  printAt(0, 36, mode == Mode::Bluetooth ? "Modo Bluetooth" : "Modo Web");
  if (mode == Mode::Bluetooth) printAt(0, 52, "Vermelho: modo Web");
  s_oled.display();
}

void display::showModeChange(Mode target) {
  if (!s_ok) return;
  s_oled.clearDisplay();
  printCentered(16, target == Mode::Web ? "Modo Web" : "Bluetooth", 2);
  printCentered(42, "reiniciando...");
  s_oled.display();
}

void display::render(const UiState &ui) {
  if (!s_ok) return;
  static uint32_t last = 0;
  if (millis() - last < cfg::oled::REFRESH_MS) return;
  last = millis();

  s_oled.clearDisplay();
  s_oled.setTextColor(SSD1306_WHITE);
  drawStatusBar(ui);
  switch (ui.screen) {
    case Screen::Player: drawPlayer(); break;
    case Screen::Library: drawLibrary(ui); break;
    case Screen::ConfirmMode: drawConfirmMode(); break;
    case Screen::WebHome: drawWebHome(); break;
  }
  s_oled.display();
}
