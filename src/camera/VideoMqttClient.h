#pragma once
#include <PubSubClient.h>
#include <WiFi.h>
#include "../config/Config.h"

/**
 * Dedicated MQTT connection used exclusively for publishing video frames.
 *
 * Video and "everything else" (commands, live audio, sensors) used to
 * share a single MQTT connection, guarded by a mutex so the camera task
 * (Core 0) and the main loop (Core 1) never touched the socket at the same
 * time. That worked, but meant video and audio were constantly waiting
 * their turn for the same channel — under load (both streaming at once)
 * this caused dropped frames and choppy audio.
 *
 * This class is a second, completely independent connection to the same
 * broker, used only by the camera task. With its own socket, there is
 * nothing to contend with — no mutex needed here.
 */
class VideoMqttClient {
public:
  void begin() {
    _client.setClient(_wifiClient);
    _client.setServer(MQTT_BROKER_HOST, MQTT_BROKER_PORT);
    _client.setBufferSize(65000); // ~64KB — JPEG frames can exceed 32KB on detailed scenes
    _client.setKeepAlive(60); // see MqttGateway.h for why 60s instead of the 15s default
    _reconnect();
  }

  /** Call every camera task iteration to keep the connection alive and reconnect if dropped. */
  void maintain() {
    if (!_client.connected()) {
      _reconnect();
      return;
    }
    _client.loop();
  }

  bool publishBytes(const char* topic, const uint8_t* data, size_t len) {
    if (!_client.connected()) return false;
    return _client.publish(topic, data, len);
  }

  bool isConnected() { return _client.connected(); }

private:
  WiFiClient    _wifiClient;
  PubSubClient  _client;
  unsigned long _lastReconnectAttempt = 0;

  void _reconnect() {
    // Rate-limited, non-blocking-ish reconnect: the camera task calls
    // maintain() every iteration, so we just try again every 3s instead of
    // blocking the whole task with delay() like the main connection does.
    unsigned long now = millis();
    if (now - _lastReconnectAttempt < 3000) return;
    _lastReconnectAttempt = now;

    Serial.printf("[MQTT-Video] Connecting to %s:%d ...\n", MQTT_BROKER_HOST, MQTT_BROKER_PORT);
    if (_client.connect(MQTT_VIDEO_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD)) {
      Serial.println("[MQTT-Video] Connected.");
    } else {
      Serial.printf("[MQTT-Video] Failed (rc=%d), will retry.\n", _client.state());
    }
  }
};
