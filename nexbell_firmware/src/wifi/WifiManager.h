#pragma once
#include <WiFi.h>

/**
 * Non-blocking Wi-Fi manager with auto-reconnect.
 */
class WifiManager {
public:
  void begin(const char* ssid, const char* password) {
    _ssid     = ssid;
    _password = password;
    WiFi.mode(WIFI_STA);
    _connect();
  }

  /** Call every loop iteration – reconnects transparently if connection drops. */
  void maintain() {
    if (WiFi.status() != WL_CONNECTED) {
      unsigned long now = millis();
      if (now - _lastAttempt > 5000) {
        Serial.println("[WiFi] Reconnecting...");
        _connect();
        _lastAttempt = now;
      }
    }
  }

  bool isConnected() const { return WiFi.status() == WL_CONNECTED; }

private:
  const char* _ssid;
  const char* _password;
  unsigned long _lastAttempt = 0;

  void _connect() {
    WiFi.disconnect(true);
    WiFi.begin(_ssid, _password);
    Serial.print("[WiFi] Connecting");
    // Block only at startup (setup()), not in maintain().
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 8000) {
      delay(250);
      Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("\n[WiFi] Connected: %s\n", WiFi.localIP().toString().c_str());
    } else {
      Serial.println("\n[WiFi] Failed – will retry.");
    }
  }
};
