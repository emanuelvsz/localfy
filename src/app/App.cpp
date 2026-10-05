#include "app/App.h"

#include <Arduino.h>

#include "audio/Player.h"
#include "config/Config.h"
#include "core/BootMode.h"
#include "core/Lock.h"
#include "storage/Library.h"
#include "ui/Button.h"
#include "ui/Display.h"
#include "ui/StatusLed.h"
#include "ui/UiState.h"
#include "ui/VolumeKnob.h"

namespace {
Button s_next(cfg::pin::BTN_NEXT, cfg::ui::LONG_PRESS_MS);
Button s_prev(cfg::pin::BTN_PREV);
Button s_play(cfg::pin::BTN_PLAY);
Button s_mode(cfg::pin::BTN_MODE);

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
// Lista de musicas
// ---------------------------------------------------------------------------
void openLibrary() {
  s_ui.screen = Screen::Library;
  int current = player::currentIndex();
  s_ui.cursor = current >= 0 ? current : 0;
  s_lastLibraryUseMs = millis();
}

void moveCursor(int delta) {
  int total = (int)library::count();
  if (total == 0) return;
  s_ui.cursor = (s_ui.cursor + delta + total) % total;
  s_lastLibraryUseMs = millis();
}

void playCursor() {
  if (library::count() == 0) return;
  Serial.printf("[UI] tocar %s\n", library::songs()[s_ui.cursor].name.c_str());
  {
    Guard g;
    player::play(s_ui.cursor);
  }
  s_ui.screen = Screen::Player;   // volta ao player para mostrar o que toca
}

// ---------------------------------------------------------------------------
// Reacao a cada botao, conforme a tela aberta
// ---------------------------------------------------------------------------
void onModeClick() {
  if (s_ui.screen == Screen::ConfirmMode) cancelModeConfirm();
  else openModeConfirm();
}

void onPlayClick() {
  switch (s_ui.screen) {
    case Screen::ConfirmMode: confirmModeChange(); break;
    case Screen::Library: playCursor(); break;
    case Screen::Player: {
      Guard g;
      player::togglePause();
      break;
    }
    case Screen::WebHome: break;   // sem Bluetooth no modo Web: nada a tocar
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
    default: break;
  }
}

void onNextLongPress() {
  if (s_ui.screen == Screen::Library) playCursor();
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
  if (next == Ev::Click) onArrowClick(+1);
  else if (next == Ev::LongPress) onNextLongPress();

  if (s_prev.poll() == Ev::Click) onArrowClick(-1);
  if (s_play.poll() == Ev::Click) onPlayClick();
  if (s_mode.poll() == Ev::Click) onModeClick();

  if (volumeknob::poll()) s_ui.volumeShownAt = millis();
  handleTimeouts();
}

void app::render() { display::render(s_ui); }
