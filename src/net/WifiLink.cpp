#include "net/WifiLink.h"

#include <WiFi.h>
#include <time.h>

#include "config/Config.h"
#include "core/Diagnostics.h"

void wifilink::off() { WiFi.mode(WIFI_OFF); }

bool wifilink::connect() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  diag::heapMark("antes Wi-Fi");
  WiFi.begin(cfg::WIFI_SSID, cfg::WIFI_PASS);

  Serial.print("[WIFI] conectando");
  uint32_t start = millis();
  while (!WiFi.isConnected() && millis() - start < cfg::net::WIFI_CONNECT_TIMEOUT_MS) {
    delay(250);
    Serial.print('.');
  }
  Serial.println();

  if (!WiFi.isConnected()) {
    Serial.println("[WIFI] falhou - seguindo offline");
    return false;
  }
  Serial.printf("[WIFI] %s\n", WiFi.localIP().toString().c_str());
  configTime(cfg::net::GMT_OFFSET_SEC, cfg::net::DST_OFFSET_SEC, cfg::net::NTP_SERVER_1,
             cfg::net::NTP_SERVER_2);
  return true;
}

bool wifilink::connected() { return WiFi.isConnected(); }

String wifilink::ip() { return WiFi.localIP().toString(); }
