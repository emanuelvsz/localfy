#include "app/App.h"

#include <Arduino.h>

#include "audio/Player.h"
#include "bluetooth/Bluetooth.h"
#include "config/Config.h"
#include "core/BootMode.h"
#include "core/Lock.h"
#include "storage/Library.h"
#include "ui/Button.h"
#include "ui/Display.h"
#include "ui/LibraryMenu.h"
#include "ui/StatusLed.h"
#include "ui/UiState.h"
#include "ui/VolumeKnob.h"

namespace {
Button s_next(cfg::pin::BTN_NEXT, cfg::ui::ARROW_HOLD_MS, cfg::ui::ARROW_REPEAT_MS);
Button s_prev(cfg::pin::BTN_PREV, cfg::ui::ARROW_HOLD_MS, cfg::ui::ARROW_REPEAT_MS);
Button s_play(cfg::pin::BTN_PLAY, cfg::ui::STANDBY_HOLD_MS);
Button s_mode(cfg::pin::BTN_MODE, cfg::ui::LONG_PRESS_MS);

UiState s_ui;
Screen s_screenBeforeConfirm = Screen::Player;
uint32_t s_lastLibraryUseMs = 0;
uint32_t s_confirmOpenedMs = 0;

Screen homeScreen() {
  return bootmode::current() == Mode::Web ? Screen::WebHome : Screen::Player;
}

// ---------------------------------------------------------------------------
// Troca de modo (botao vermelho + confirmacao no verde)
// ---------------------------------------------------------------------------
void openModeConfirm() {
  s_screenBeforeConfirm = s_ui.screen;
  s_ui.screen = Screen::ConfirmMode;
  s_confirmOpenedMs = millis();
  Serial.printf("[UI] trocar para o modo %s? (verde confirma)\n",
                bootmode::label(bootmode::other()));
}

void cancelModeConfirm() {
  s_ui.screen = s_screenBeforeConfirm;
  Serial.println("[UI] troca de modo cancelada");
}

void confirmModeChange() {
  Mode target = bootmode::other();
  {
    Guard g;
    player::stop();   // para a musica e grava o historico antes de reiniciar
  }
  statusled::set(target == Mode::Web);
  display::showModeChange(target);
  bootmode::restartInto(target);
}

// ---------------------------------------------------------------------------
// Standby (segurar o verde): mostra a cara do BMO. A musica continua como
// estava, tocando ou pausada.
// ---------------------------------------------------------------------------
void enterStandby() {
  s_ui.screen = Screen::Standby;
  Serial.println("[UI] standby");
}

// Volta sempre para a tela principal do modo, nunca para a lista ou a
// pergunta de troca de modo que estivessem abertas antes.
void exitStandby() {
  s_ui.screen = homeScreen();
  Serial.println("[UI] acordou do standby");
}

// ---------------------------------------------------------------------------
// Escolher o fone (segurar o vermelho no modo Bluetooth)
// ---------------------------------------------------------------------------
void openConnections() {
  if (bootmode::current() != Mode::Bluetooth) return;   // modo Web: rede fixa
  s_ui.screen = Screen::BtPicker;
  s_ui.cursor = 0;
  bluetooth::beginPicker();
}

void movePickerCursor(int delta) {
  int total = (int)bluetooth::devices().size();
  if (total == 0) return;
  s_ui.cursor = (s_ui.cursor + delta + total) % total;
}

void closePicker(bool connect) {
  if (connect && !bluetooth::devices().empty()) bluetooth::pick(s_ui.cursor);
  else bluetooth::cancelPicker();
  s_ui.screen = Screen::Player;
}

// ---------------------------------------------------------------------------
// Lista de musicas: nivel 0 = pastas (e o aleatorio), nivel 1 = musicas
// ---------------------------------------------------------------------------
void openFolder(int filter) {
  s_ui.libLevel = 1;
  s_ui.libFilter = filter;
  int pos = libmenu::posOf(filter, player::currentIndex());
  s_ui.cursor = pos >= 0 ? pos : 0;
}

void openFolderList() {
  s_ui.libLevel = 0;
  s_ui.cursor = libmenu::itemOf(s_ui.libFilter);
}

// Pelas setas no player: abre direto na pasta da fila, na faixa atual.
void openLibrary() {
  s_ui.screen = Screen::Library;
  if (library::countIn(player::queueFilter()) > 0) openFolder(player::queueFilter());
  else openFolderList();
  s_lastLibraryUseMs = millis();
}

int libraryItems() {
  return s_ui.libLevel == 0 ? libmenu::folderItems() : (int)library::countIn(s_ui.libFilter);
}

void moveCursor(int delta) {
  int total = libraryItems();
  if (total == 0) return;
  s_ui.cursor = (s_ui.cursor + delta + total) % total;
  s_lastLibraryUseMs = millis();
}

void playCursor() {
  int idx = libmenu::songAt(s_ui.libFilter, s_ui.cursor);
  if (idx < 0) return;
  Serial.printf("[UI] tocar %s\n", library::nameOf(idx).c_str());
  {
    Guard g;
    // A pasta aberta vira a fila: proxima/anterior ficam dentro dela.
    player::setQueue(s_ui.libFilter, player::shuffle());
    player::play(idx);
  }
  s_ui.screen = Screen::Player;   // volta ao player para mostrar o que toca
}

// Verde na lista.
void libraryConfirm() {
  s_lastLibraryUseMs = millis();
  if (s_ui.libLevel == 1) {
    playCursor();
  } else if (s_ui.cursor == 0) {
    Guard g;
    player::setQueue(player::queueFilter(), !player::shuffle());
  } else {
    openFolder(libmenu::filterAt(s_ui.cursor));
  }
}

// Vermelho na lista: sobe um nivel.
void libraryBack() {
  s_lastLibraryUseMs = millis();
  if (s_ui.libLevel == 1) openFolderList();
  else s_ui.screen = Screen::Player;
}

// ---------------------------------------------------------------------------
// Reacao a cada botao, conforme a tela aberta
// ---------------------------------------------------------------------------
void onModeClick() {
  if (s_ui.screen == Screen::ConfirmMode) cancelModeConfirm();
  else if (s_ui.screen == Screen::BtPicker) closePicker(false);
  else if (s_ui.screen == Screen::Library) libraryBack();
  else openModeConfirm();
}

void onModeLongPress() {
  if (s_ui.screen == Screen::ConfirmMode) cancelModeConfirm();
  if (s_ui.screen != Screen::BtPicker) openConnections();
}

void onPlayClick() {
  switch (s_ui.screen) {
    case Screen::ConfirmMode: confirmModeChange(); break;
    case Screen::Library: libraryConfirm(); break;
    case Screen::Player: {
      Guard g;
      player::togglePause();
      break;
    }
    case Screen::WebHome: break;   // sem Bluetooth no modo Web: nada a tocar
    case Screen::Standby: break;   // tratado antes, em app::update()
    case Screen::BtPicker: closePicker(true); break;
  }
}

// Azuis: no player, o primeiro toque so abre a lista (evita trocar de musica
// sem querer); na lista, movem o cursor.
void onArrowClick(int direction) {
  switch (s_ui.screen) {
    case Screen::Player:
      if (library::count() > 0) openLibrary();
      break;
    case Screen::Library: moveCursor(direction); break;
    case Screen::BtPicker: movePickerCursor(direction); break;
    default: break;
  }
}

// Segurar um azul na lista: vai para o inicio da pagina seguinte/anterior
// (dando a volta). Com 100+ pastas, ir de uma em uma era toque demais.
void onArrowHold(int direction) {
  if (s_ui.screen != Screen::Library) return;
  int total = libraryItems();
  if (total == 0) return;
  const int rows = cfg::oled::LIST_ROWS;
  int pages = (total + rows - 1) / rows;
  int page = (s_ui.cursor / rows + direction + pages) % pages;
  s_ui.cursor = page * rows;
  s_lastLibraryUseMs = millis();
}

void onPlayLongPress() {
  if (s_ui.screen != Screen::ConfirmMode && s_ui.screen != Screen::BtPicker) enterStandby();
}

void handleTimeouts() {
  uint32_t now = millis();
  if (s_ui.screen == Screen::Library &&
      now - s_lastLibraryUseMs > cfg::ui::LIBRARY_TIMEOUT_MS) {
    s_ui.screen = Screen::Player;
  }
  if (s_ui.screen == Screen::ConfirmMode &&
      now - s_confirmOpenedMs > cfg::ui::CONFIRM_TIMEOUT_MS) {
    cancelModeConfirm();
  }
}
}  // namespace

void app::begin() {
  s_next.begin();
  s_prev.begin();
  s_play.begin();
  s_mode.begin();
  Serial.printf("[BTN] proxima=GPIO%d anterior=GPIO%d play=GPIO%d modo=GPIO%d\n",
                cfg::pin::BTN_NEXT, cfg::pin::BTN_PREV, cfg::pin::BTN_PLAY, cfg::pin::BTN_MODE);
  s_ui.screen = homeScreen();
}

void app::update() {
  using Ev = Button::Event;

  Ev next = s_next.poll();
  Ev prev = s_prev.poll();
  Ev play = s_play.poll();
  Ev mode = s_mode.poll();
  bool knobMoved = volumeknob::poll();
  if (knobMoved) s_ui.volumeShownAt = millis();

  // No standby so o verde acorda, e esse clique nao pausa a musica. Os outros
  // botoes sao ignorados; o knob continua mudando o volume.
  if (s_ui.screen == Screen::Standby) {
    if (play == Ev::Click) exitStandby();
    return;
  }

  if (next == Ev::Click) onArrowClick(+1);
  else if (next == Ev::LongPress) onArrowHold(+1);

  if (prev == Ev::Click) onArrowClick(-1);
  else if (prev == Ev::LongPress) onArrowHold(-1);
  if (play == Ev::Click) onPlayClick();
  else if (play == Ev::LongPress) onPlayLongPress();
  if (mode == Ev::Click) onModeClick();
  else if (mode == Ev::LongPress) onModeLongPress();

  handleTimeouts();
}

void app::render() { display::render(s_ui); }
