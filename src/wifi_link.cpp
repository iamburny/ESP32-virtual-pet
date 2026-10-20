#include "wifi_link.h"

#include <WiFi.h>

#include "secrets.h"

bool wifi_up(uint32_t timeout_ms) {
  if (WiFi.status() == WL_CONNECTED) return true;
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < timeout_ms) delay(100);
  bool ok = WiFi.status() == WL_CONNECTED;
  if (ok)
    Serial.printf("[wifi] connected, IP %s\n", WiFi.localIP().toString().c_str());
  else
    Serial.println("[wifi] connect timed out");
  return ok;
}

void wifi_down() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}

bool wifi_is_connected() { return WiFi.status() == WL_CONNECTED; }
