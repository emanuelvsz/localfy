#include "bluetooth/Bluetooth.h"

#include <esp_gap_bt_api.h>

#include "BluetoothA2DPSource.h"
#include "audio/AudioPipeline.h"
#include "audio/Player.h"
#include "config/Config.h"
#include "core/BootMode.h"
#include "core/Diagnostics.h"
#include "storage/Settings.h"

namespace {
// Usamos a BluetoothA2DPSource diretamente, e nao o envelope A2DPStream do
// arduino-audio-tools: com o envelope a conexao caia logo apos "Connecting".
BluetoothA2DPSource s_a2dp;

volatile BtState s_state = BtState::Disconnected;
String s_name, s_address;
String s_connectTo;              // nome ou MAC pedido pela interface ("" = padrao)
bool s_radioOn = false;
volatile bool s_startRequested = false;
volatile bool s_picking = false;   // seletor aberto: lista tudo e nao conecta

// Aparelhos vistos, preenchidos pela task do Bluetooth. Tamanho fixo de
// proposito: um std::vector aqui corrompia a memoria, porque o push_back
// realocava enquanto a task do servidor web lia o mesmo vetor.
struct Found {
  char name[32];
  char mac[18];
  int rssi;
};
volatile int s_foundCount = 0;
Found s_found[cfg::bt::MAX_DEVICES];
std::vector<BtDevice> s_devices;   // copia lida pelas rotas web

// Prioridade: pedido da interface > ultimo aparelho escolhido (pelo MAC, que
// nao muda) > nome de secrets.h.
String currentTarget() {
  if (!s_connectTo.isEmpty()) return s_connectTo;
  String saved = settings::btAddress();
  return saved.isEmpty() ? String(cfg::BT_TARGET) : saved;
}

// A biblioteca so procura aparelhos ate a primeira conexao; depois que um fone
// desconecta ela fica parada. Esta chamada retoma a busca (o callback abaixo
// decide em quem conectar). Se ja houver uma busca em andamento, o pedido e
// recusado sem efeito.
void startDiscovery() {
  if (!s_radioOn) return;
  esp_err_t err = esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, 10, 0);
  Serial.printf("[BT] busca %s\n", err == ESP_OK ? "iniciada" : "ja em andamento");
}

// Chamado pela pilha Bluetooth pedindo 'count' quadros estereo de 16 bits.
int32_t onAudioData(Frame *frames, int32_t count) {
  pipeline::fill(reinterpret_cast<uint8_t *>(frames), (size_t)count * sizeof(Frame));
  return count;
}

// Chamado para cada aparelho encontrado. Retornar true faz a biblioteca
// conectar nele; false segue procurando.
bool onDeviceFound(const char *ssid, esp_bd_addr_t addr, int rssi) {
  char mac[18];
  snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X",
           addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);

  int n = s_foundCount;
  bool seen = false;
  for (int i = 0; i < n; i++)
    if (strcmp(s_found[i].mac, mac) == 0) seen = true;

  if (!seen && n < (int)cfg::bt::MAX_DEVICES) {
    snprintf(s_found[n].name, sizeof(s_found[n].name), "%s",
             (ssid && *ssid) ? ssid : "(sem nome)");
    snprintf(s_found[n].mac, sizeof(s_found[n].mac), "%s", mac);
    s_found[n].rssi = rssi;
    s_foundCount = n + 1;   // publica so depois de preencher
    Serial.printf("[BT] achou %s (%s) %d dBm\n", s_found[n].name, mac, rssi);
  }

  // Correspondencia tolerante: ignora maiusculas e aceita parte do nome. O
  // mesmo aparelho anuncia "JBL WAVE FLEX" e "JBL WAVE FLEX-LE"; a variante
  // LE e Bluetooth Low Energy, que nao transporta audio, e e descartada.
  // Com o seletor aberto so listamos; a conexao espera a escolha.
  if (s_picking) return false;

  String target = currentTarget();
  if (target.isEmpty()) return false;
  String name = ssid ? String(ssid) : String("");
  String targetUp = target;
  targetUp.toUpperCase();
  String nameUp = name;
  nameUp.toUpperCase();

  bool match = (!nameUp.isEmpty() && nameUp.indexOf(targetUp) >= 0 && !nameUp.endsWith("-LE")) ||
               String(mac).equalsIgnoreCase(target);
  if (match) Serial.printf("[BT] ALVO ENCONTRADO: %s (%s)\n", name.c_str(), mac);
  return match;
}

// Copia os aparelhos vistos para o vetor lido pelas rotas. Roda no loop
// principal, onde nao concorre com a task do Bluetooth.
void collectFound() {
  int n = s_foundCount;
  if ((int)s_devices.size() >= n) return;
  s_devices.clear();
  for (int i = 0; i < n; i++)
    s_devices.push_back({String(s_found[i].name), String(s_found[i].mac), s_found[i].rssi});
}

void startRadioIfRequested() {
  if (!s_startRequested) return;
  s_startRequested = false;
  if (s_radioOn || bootmode::current() != Mode::Bluetooth) return;

  // O ponteiro do nome precisa sobreviver a chamada start().
  static String target;
  target = currentTarget();
  Serial.printf("[BT] ligando radio, alvo: %s\n", target.c_str());

  s_a2dp.set_ssid_callback(onDeviceFound);
  // Reconexao automatica desligada: a biblioteca guarda o MAC do ultimo
  // aparelho e passa a ignorar o nome configurado. Trocar de fone exigiria
  // apagar a memoria interna a cada vez.
  s_a2dp.set_auto_reconnect(false);
  // O buffer PCM so e criado depois da conexao (ver poll()): criar antes
  // deixava pouca RAM e a inicializacao do Bluetooth falhava.
  s_a2dp.start(target.c_str(), onAudioData);
  s_radioOn = true;
  diag::heapMark("apos BT start");
  Serial.println("[BT] radio ligado, procurando");
}
}  // namespace

void bluetooth::requestStart() {
  s_state = BtState::Connecting;
  s_name = settings::btName();
  s_address = settings::btAddress();
  s_startRequested = true;
  Serial.printf("[BT] procurando %s\n", currentTarget().c_str());
}

void bluetooth::beginPicker() {
  player::stop();
  s_picking = true;
  s_devices.clear();
  s_foundCount = 0;
  bool wasConnected = s_a2dp.is_connected();
  s_state = BtState::Scanning;
  if (wasConnected) s_a2dp.disconnect();
  startDiscovery();
  Serial.println("[BT] seletor aberto");
}

bool bluetooth::pick(size_t index) {
  if (index >= s_devices.size()) return false;
  const BtDevice &d = s_devices[index];
  settings::setBt(d.name, d.address);
  s_name = d.name;
  s_address = d.address;
  s_connectTo = d.address;
  s_picking = false;
  s_state = BtState::Connecting;
  startDiscovery();   // normalmente ja esta rodando; o callback conecta ao rever o MAC
  Serial.printf("[BT] escolhido %s (%s)\n", d.name.c_str(), d.address.c_str());
  return true;
}

void bluetooth::cancelPicker() {
  s_picking = false;
  s_state = BtState::Connecting;
  startDiscovery();
  Serial.println("[BT] seletor cancelado, procurando o aparelho salvo");
}

bool bluetooth::picking() { return s_picking; }

void bluetooth::requestConnect(const String &name, const String &address) {
  s_state = BtState::Connecting;
  s_name = name;
  s_address = address;
  s_connectTo = name.isEmpty() ? address : name;
  s_startRequested = true;
}

// A biblioteca ESP32-A2DP nao tem um modo "so escanear": a descoberta
// acontece durante a tentativa de conexao, e o callback registra cada
// aparelho visto.
void bluetooth::requestScan(uint32_t /*seconds*/) {
  s_devices.clear();
  s_connectTo = "";
  s_foundCount = 0;
  s_state = BtState::Connecting;
  s_startRequested = true;
}

void bluetooth::disconnect() {
  player::stop();
  // disconnect(), e nao end(): end() libera a pilha Bluedroid, que nao pode
  // ser iniciada de novo sem reiniciar a placa.
  s_a2dp.disconnect();
  s_state = BtState::Disconnected;
  s_name = "";
  s_address = "";
}

void bluetooth::poll() {
  startRadioIfRequested();
  collectFound();

  if (s_state == BtState::Connecting && s_a2dp.is_connected()) {
    s_state = BtState::Connected;
    Serial.println("[BT] CONECTADO");
    bool ok = pipeline::createBuffer();
    Serial.printf("[BT] buffer de audio %s, heap livre %u\n", ok ? "criado" : "FALHOU",
                  (unsigned)ESP.getFreeHeap());
    }

  if (s_state == BtState::Connected && !s_a2dp.is_connected()) {
    // Fone desligou ou saiu de alcance: para o audio e volta a procurar o
    // mesmo aparelho, para reconectar sozinho quando ele voltar.
    player::onLinkLost();
    s_state = BtState::Connecting;
    startDiscovery();
  }
}

BtState bluetooth::state() { return s_state; }

const char *bluetooth::stateName() {
  switch (s_state) {
    case BtState::Connected: return "connected";
    case BtState::Connecting: return "connecting";
    case BtState::Scanning: return "scanning";
    default: return "disconnected";
  }
}

bool bluetooth::isConnected() { return s_state == BtState::Connected; }
const String &bluetooth::deviceName() { return s_name; }
const String &bluetooth::deviceAddress() { return s_address; }
const std::vector<BtDevice> &bluetooth::devices() { return s_devices; }

