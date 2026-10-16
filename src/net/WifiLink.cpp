#include "net/WifiLink.h"

#include <DNSServer.h>
#include <WiFi.h>

#include "config/Config.h"
#include "core/Diagnostics.h"

namespace {
DNSServer s_dns;
bool s_up = false;
}  // namespace

void wifilink::off() { WiFi.mode(WIFI_OFF); }

bool wifilink::startAccessPoint() {
  WiFi.mode(WIFI_AP);
  diag::heapMark("antes Wi-Fi");
  s_up = WiFi.softAP(cfg::net::AP_SSID, cfg::net::AP_PASS);
  if (!s_up) {
    Serial.println("[WIFI] falha ao criar a rede");
    return false;
  }
  // Qualquer endereco pesquisado aponta para o Localfy: ao entrar na rede, o
  // celular mostra o aviso "entrar na rede" e abre a interface sozinho.
  s_dns.start(53, "*", WiFi.softAPIP());
  Serial.printf("[WIFI] rede %s no ar, abra http://%s\n", cfg::net::AP_SSID,
                WiFi.softAPIP().toString().c_str());
  return true;
}

bool wifilink::up() { return s_up; }
const char *wifilink::networkName() { return cfg::net::AP_SSID; }
const char *wifilink::password() { return cfg::net::AP_PASS; }
String wifilink::ip() { return WiFi.softAPIP().toString(); }
int wifilink::clients() { return s_up ? WiFi.softAPgetStationNum() : 0; }

void wifilink::poll() {
  if (s_up) s_dns.processNextRequest();
}
