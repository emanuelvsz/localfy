#include "storage/Settings.h"

#include <Preferences.h>

namespace {
constexpr const char *NS = "localfy";

// Lido uma vez e mantido em RAM: a busca Bluetooth consulta o alvo a cada
// aparelho encontrado, e abrir a NVS toda vez fragmentava a memoria.
bool s_loaded = false;
String s_btName, s_btAddr;
String s_qFolder;
bool s_qShuffle = false;

void load() {
  if (s_loaded) return;
  Preferences p;
  // Leitura e escrita: na primeira vez cria o espaco "localfy" sem erro.
  if (p.begin(NS, false)) {
    s_btName = p.getString("bt_name", "");
    s_btAddr = p.getString("bt_addr", "");
    s_qFolder = p.getString("q_folder", "");
    s_qShuffle = p.getBool("q_shuffle", false);
    p.end();
  }
  s_loaded = true;
}
}  // namespace

String settings::btName() {
  load();
  return s_btName;
}

String settings::btAddress() {
  load();
  return s_btAddr;
}

void settings::setBt(const String &name, const String &address) {
  load();
  s_btName = name;
  s_btAddr = address;
  Preferences p;
  if (p.begin(NS, false)) {
    p.putString("bt_name", name);
    p.putString("bt_addr", address);
    p.end();
  }
  Serial.printf("[CFG] Bluetooth salvo: %s (%s)\n", name.c_str(), address.c_str());
}

String settings::queueFolder() {
  load();
  return s_qFolder;
}

bool settings::queueShuffle() {
  load();
  return s_qShuffle;
}

void settings::setQueue(const String &folder, bool shuffle) {
  load();
  if (folder == s_qFolder && shuffle == s_qShuffle) return;
  s_qFolder = folder;
  s_qShuffle = shuffle;
  Preferences p;
  if (p.begin(NS, false)) {
    p.putString("q_folder", folder);
    p.putBool("q_shuffle", shuffle);
    p.end();
  }
}
