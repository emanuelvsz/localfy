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
#include "ui/LibraryMenu.h"

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
  String t = library::fileName(file);   // sem a pasta do artista
  int dot = t.lastIndexOf('.');
  if (dot > 0) t = t.substring(0, dot);
  // Sufixo de site de download ("_spotdown.org"). So corta se o pedaco depois
  // do "_" parecer um dominio: um "_" de verdade no titulo fica.
  int suffix = t.lastIndexOf('_');
  if (suffix > 3 && t.indexOf('.', suffix) > 0 && t.indexOf(' ', suffix) < 0)
    t = t.substring(0, suffix);
  t.trim();
  if ((int)t.length() > maxChars) t = t.substring(0, maxChars - 1) + ".";
  return t;
}

// Corta um texto qualquer (nome de pasta) com ponto final se nao couber.
String fit(const String &text, int maxChars) {
  if ((int)text.length() <= maxChars) return text;
  return text.substring(0, maxChars - 1) + ".";
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
  } else if (ui.screen == Screen::BtPicker) {
    left = "ESCOLHER FONE";
  } else if (web) {
    int n = wifilink::clients();
    left = !wifilink::up() ? String("Wi-Fi falhou")
           : n == 0        ? String("Rede no ar")
                           : String(n) + (n == 1 ? " conectado" : " conectados");
  } else if (bluetooth::isConnected()) {
    // Mostra em qual aparelho esta tocando, ja que agora da para trocar.
    const String &name = bluetooth::deviceName();
    left = name.isEmpty() ? String("BT conectado") : shortTitle(name, 16);
  } else {
    left = "BT buscando...";
  }

  printAt(0, 0, left);
  if (showModeTag) printRight(0, String(player::shuffle() ? "ALE " : "") + (web ? "WEB" : "BT"));
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

  // Artista em cima (o cadastrado na interface ou, sem cadastro, a pasta) e o
  // titulo embaixo. No aleatorio, com faixas de varios artistas, e isso que
  // diz de quem e cada musica.
  const Song &song = library::songs()[idx];
  String name = library::nameOf(idx);
  const Meta *m = meta::find(name);
  String artist = m && !m->artist.isEmpty() ? m->artist
                  : song.folder >= 0        ? String(library::folderName(song.folder))
                                            : String("");
  int y = 14;
  if (!artist.isEmpty()) {
    printAt(0, y, fit(artist, LINE_CHARS));
    s_oled.drawFastHLine(0, y + 9, (int)fit(artist, LINE_CHARS).length() * CHAR_W, SSD1306_WHITE);
    y += 12;
  }
  String title = shortTitle(name, LINE_CHARS * 2);
  printAt(0, y, title.substring(0, LINE_CHARS));
  if ((int)title.length() > LINE_CHARS) printAt(0, y + 10, title.substring(LINE_CHARS));

  // A duracao de MP3 VBR nao e confiavel: so mostra o total quando existe.
  uint32_t pos = player::positionSecs();
  uint32_t dur = player::durationSecs();
  printAt(0, 50, player::isPlaying() ? ">" : "||");
  printAt(14, 50, dur > 0 ? formatTime(pos) + "/" + formatTime(dur) : formatTime(pos));
  printRight(50, String(idx + 1) + "/" + String(library::count()));
  if (dur > 0) s_oled.drawFastHLine(0, 62, (int)((uint64_t)pos * WIDTH / dur), SSD1306_WHITE);
}

// Uma linha da lista, destacada em barra invertida se selecionada.
void drawListRow(int row, const String &text, bool selected, const String &right = "") {
  int y = 12 + row * 10;   // abaixo da trilha "Biblioteca > Pasta"
  if (selected) s_oled.fillRect(0, y - 1, WIDTH, 10, SSD1306_WHITE);
  s_oled.setTextColor(selected ? SSD1306_BLACK : SSD1306_WHITE);
  printAt(2, y, text);
  if (!right.isEmpty()) printAt(WIDTH - 1 - (int)right.length() * CHAR_W, y, right);
  s_oled.setTextColor(SSD1306_WHITE);
}

// Trilha no topo: "Biblioteca" ou "Biblioteca > Joji" (encurta a pasta se
// nao couber).
void drawBreadcrumb(const String &folder) {
  String path = "Biblioteca";
  if (!folder.isEmpty()) {
    String prefix = "Biblioteca > ";
    path = (int)(prefix.length() + folder.length()) <= LINE_CHARS ? prefix + folder
                                                                  : "< " + fit(folder, LINE_CHARS - 2);
  }
  printAt(0, 0, path);
  s_oled.drawFastHLine(0, 9, WIDTH, SSD1306_WHITE);
}

void drawListFooter(int cursor, int total) {
  const int rows = cfg::oled::LIST_ROWS;
  s_oled.drawFastHLine(0, 53, WIDTH, SSD1306_WHITE);
  printAt(0, 56, "pag " + String(cursor / rows + 1) + "/" + String((total + rows - 1) / rows));
  printRight(56, String(cursor + 1) + "/" + String(total));
}

// Lista sem a barra de status: trilha no topo, 4 itens e a paginacao.
// Nivel 0: aleatorio + pastas. Nivel 1: musicas da pasta aberta.
void drawLibrary(const UiState &ui) {
  const int rows = cfg::oled::LIST_ROWS;
  if (library::count() == 0) {
    printAt(0, 28, "Biblioteca vazia");
    return;
  }
  int first = ui.cursor / rows * rows;

  if (ui.libLevel == 0) {
    int total = libmenu::folderItems();
    for (int i = 0; i < rows && first + i < total; i++) {
      int item = first + i;
      bool sel = item == ui.cursor;
      if (item == 0) {
        drawListRow(i, "Aleatorio", sel, player::shuffle() ? "SIM" : "NAO");
      } else {
        int f = libmenu::filterAt(item);
        String label = library::filterLabel(f);
        bool queued = f == player::queueFilter();
        drawListRow(i, (queued ? ">" : " ") + fit(label, 13),
                    sel, String(library::countIn(f)));
      }
    }
    drawBreadcrumb("");
    drawListFooter(ui.cursor, total);
    return;
  }

  int total = (int)library::countIn(ui.libFilter);
  for (int i = 0; i < rows && first + i < total; i++) {
    int idx = libmenu::songAt(ui.libFilter, first + i);
    bool playing = idx == player::currentIndex();
    drawListRow(i, shortTitle(library::fileOf(idx), playing ? 18 : 20),
                first + i == ui.cursor, playing ? ">" : "");
  }
  drawBreadcrumb(library::filterLabel(ui.libFilter));
  drawListFooter(ui.cursor, total);
}

// Pergunta aberta pelo botao vermelho.
void drawConfirmMode() {
  Mode target = bootmode::other();
  printAt(0, 14, "Ir para o modo:");
  printCentered(25, target == Mode::Web ? "WEB" : "BLUETOOTH", 2);
  printAt(0, 45, "Verde:    confirmar");
  printAt(0, 55, "Vermelho: cancelar");
}

// Cara do BMO no standby, nas proporcoes do desenho: olhos de pontinho e
// boca pequena logo abaixo deles. Pisca a cada ~4 s e troca de expressao a
// cada ~7 s.
enum class Face : uint8_t { Smile, Serene, Grin, Surprised };

constexpr int EYE_Y = 22;
constexpr int EYE_LX = 44;
constexpr int EYE_RX = 84;
constexpr int MOUTH_Y = 31;

// Curva em "U" centrada em cx: y = topo + fundo * (1 - (x/w)^2), 2 px de espessura.
void drawU(int cx, int top, int depth, int halfW) {
  for (int dx = -halfW; dx <= halfW; dx++) {
    float k = (float)dx / halfW;
    int y = top + (int)(depth * (1.0f - k * k) + 0.5f);
    s_oled.drawPixel(cx + dx, y, SSD1306_WHITE);
    s_oled.drawPixel(cx + dx, y + 1, SSD1306_WHITE);
  }
}

void drawEyes(bool blink, bool closed) {
  for (int x : {EYE_LX, EYE_RX}) {
    if (closed) drawU(x, EYE_Y - 2, 3, 5);                   // fechados e tranquilos
    else if (blink) s_oled.fillRect(x - 4, EYE_Y, 9, 2, SSD1306_WHITE);
    else s_oled.fillCircle(x, EYE_Y, 3, SSD1306_WHITE);
  }
}

// Boca aberta em "D" deitado: reta em cima, curva embaixo, preenchida.
void drawGrin(int top, int depth, int halfW) {
  for (int dx = -halfW; dx <= halfW; dx++) {
    float k = (float)dx / halfW;
    int h = (int)(depth * sqrtf(1.0f - k * k) + 0.5f);
    s_oled.drawFastVLine(64 + dx, top, h + 1, SSD1306_WHITE);
  }
}

void drawBmoFace() {
  uint32_t now = millis();
  Face face = static_cast<Face>((now / 7000) % 4);
  bool blink = (now % 4000) < 180;

  drawEyes(blink, face == Face::Serene);
  switch (face) {
    case Face::Smile:
    case Face::Serene: drawU(64, MOUTH_Y, 3, 8); break;
    case Face::Grin: drawGrin(MOUTH_Y - 1, 6, 8); break;
    case Face::Surprised: s_oled.drawCircle(64, MOUTH_Y + 2, 3, SSD1306_WHITE); break;
  }
}

// Seletor de fones: aparelhos de audio encontrados, o destacado em barra
// invertida. A lista cresce enquanto a busca roda.
void drawBtPicker(const UiState &ui) {
  const auto &devs = bluetooth::devices();
  if (devs.empty()) {
    printCentered(22, "Procurando...");
    printCentered(34, "ligue o fone ou a");
    printCentered(44, "caixa em pareamento");
    printAt(0, 56, "Vermelho: cancelar");
    return;
  }
  const int rows = 3;
  int total = (int)devs.size();
  int first = ui.cursor / rows * rows;
  for (int i = 0; i < rows && first + i < total; i++) {
    int idx = first + i;
    int y = 14 + i * 11;
    bool selected = idx == ui.cursor;
    if (selected) s_oled.fillRect(0, y - 2, WIDTH, 11, SSD1306_WHITE);
    s_oled.setTextColor(selected ? SSD1306_BLACK : SSD1306_WHITE);
    printAt(2, y, shortTitle(devs[idx].name, 20));
  }
  s_oled.setTextColor(SSD1306_WHITE);
  s_oled.drawFastHLine(0, 47, WIDTH, SSD1306_WHITE);
  printAt(0, 50, "Verde: conectar");
  printRight(50, String(ui.cursor + 1) + "/" + String(total));
}

// Tela do modo Web: onde abrir a interface no navegador.
void drawWebHome() {
  if (!wifilink::up()) {
    printCentered(24, "Wi-Fi falhou");
    printAt(0, 56, "Vermelho: Bluetooth");
    return;
  }
  // Tudo o que o celular precisa para entrar na rede e abrir a interface.
  printAt(0, 14, "Rede:  " + String(wifilink::networkName()));
  printAt(0, 24, "Senha: " + String(wifilink::password()));
  printAt(0, 36, "Abra no navegador:");
  printAt(0, 46, "http://" + wifilink::ip());
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
  static bool inverted = false;
  bool standby = ui.screen == Screen::Standby;
  uint32_t interval = standby ? cfg::ui::STANDBY_REFRESH_MS : cfg::oled::REFRESH_MS;
  if (millis() - last < interval) return;
  last = millis();

  // Standby com cores invertidas: fundo claro e cara escura, como a tela do
  // BMO. A inversao e do proprio OLED, entao o desenho continua o mesmo.
  // (Sem dim(): ele zera o contraste e em varios modulos a tela some.)
  if (standby != inverted) {
    s_oled.invertDisplay(standby);
    inverted = standby;
  }
  if (standby) {
    s_oled.clearDisplay();
    drawBmoFace();
    s_oled.display();
    return;
  }

  s_oled.clearDisplay();
  s_oled.setTextColor(SSD1306_WHITE);
  // A lista de musicas usa a tela inteira (a barra de cima cortava a paginacao).
  if (ui.screen != Screen::Library) drawStatusBar(ui);
  switch (ui.screen) {
    case Screen::Player: drawPlayer(); break;
    case Screen::Library: drawLibrary(ui); break;
    case Screen::ConfirmMode: drawConfirmMode(); break;
    case Screen::WebHome: drawWebHome(); break;
    case Screen::Standby: break;   // desenhado acima
    case Screen::BtPicker: drawBtPicker(ui); break;
  }
  s_oled.display();
}
