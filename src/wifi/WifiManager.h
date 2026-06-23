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
    // ESP32 WiFi power-save (modem sleep) periodically puts the radio to
    // sleep between beacon intervals to save power. Under the constant
    // high-throughput traffic this device now produces (camera + live
    // audio), that's a well-known cause of routers/APs deauthenticating
    // the device with reason "AUTH_EXPIRE" — which looked like random
    // WiFi drops cascading into MQTT reconnects and lost commands.
    WiFi.setSleep(false);
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
    WiFi.setSleep(false); // re-assert after each (re)connect — some core versions reset this
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
