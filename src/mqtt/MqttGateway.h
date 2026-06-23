#pragma once
#include <PubSubClient.h>
#include <WiFi.h>
#include "../config/Config.h"
#include "../camera/CameraCapture.h"
#include "../audio/AudioPlayback.h"
#include "../audio/AudioCapture.h"

/**
 * Main MQTT connection: commands (unlock, camera start/stop signaling),
 * live audio (capture + playback), and sensor telemetry. All of this runs
 * sequentially on the main loop (Core 1) — nothing else touches this
 * connection concurrently, so no mutex is needed here.
 *
 * Video frames are published over a separate, dedicated connection
 * (VideoMqttClient, used only by the camera task on Core 0) — see that
 * class for why video was split out.
 */
class MqttGateway {
public:
  void begin() {
    _client.setClient(_wifiClient);
    _client.setServer(MQTT_BROKER_HOST, MQTT_BROKER_PORT);
    _client.setCallback([this](char* topic, byte* payload, unsigned int length) {
      _onMessage(topic, payload, length);
    });
    _client.setBufferSize(4096); // commands + audio chunks are small; video no longer goes through here
    // PubSubClient's default keepalive is only 15s. With sensors, audio
    // capture and command handling all sharing this loop, occasionally
    // missing that tight window caused the broker to silently drop the
    // connection ("exceeded timeout") — and any command published during
    // that gap (like a STOP) was lost with nobody subscribed to receive it.
    _client.setKeepAlive(60);
    _reconnect();
  }

  /** Call after begin() to register the camera module for capture triggers. */
  void setCamera(CameraCapture* cam) { _camera = cam; }

  /** Call after begin() to register the audio playback module. */
  void setAudioPlayback(AudioPlayback* ap) { _audioPlayback = ap; }

  /** Call after begin() to register the live mic capture module. */
  void setAudioCapture(AudioCapture* ac) { _audioCapture = ac; }

  /** Keep connection alive, process inbound messages. Called from the main loop (Core 1). */
  void maintain() {
    if (!_client.connected()) {
      _reconnect();
    }
    _client.loop();
  }

  /** Publish a string payload to a topic. */
  bool publish(const char* topic, const char* payload) {
    return _client.connected() && _client.publish(topic, payload);
  }

  /** Publish raw binary bytes (for audio chunks). */
  bool publishBytes(const char* topic, const uint8_t* data, size_t len) {
    return _client.connected() && _client.publish(topic, data, len);
  }

  bool isConnected() { return _client.connected(); }

private:
  WiFiClient    _wifiClient;
  PubSubClient  _client;
  CameraCapture* _camera = nullptr;
  AudioPlayback* _audioPlayback = nullptr;
  AudioCapture*  _audioCapture = nullptr;

  void _reconnect() {
    int attempts = 0;
    while (!_client.connected() && attempts < 3) {
      Serial.printf("[MQTT] Connecting to %s:%d ...\n", MQTT_BROKER_HOST, MQTT_BROKER_PORT);
      if (_client.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD)) {
        Serial.println("[MQTT] Connected.");
        _client.subscribe(TOPIC_UNLOCK_CMD);
        Serial.printf("[MQTT] Subscribed to %s\n", TOPIC_UNLOCK_CMD);
        _client.subscribe(TOPIC_CAMERA_TRIGGER);
        Serial.printf("[MQTT] Subscribed to %s\n", TOPIC_CAMERA_TRIGGER);
        _client.subscribe(TOPIC_AUDIO_PLAYBACK);
        Serial.printf("[MQTT] Subscribed to %s\n", TOPIC_AUDIO_PLAYBACK);
        _client.subscribe(TOPIC_AUDIO_START);
        Serial.printf("[MQTT] Subscribed to %s\n", TOPIC_AUDIO_START);
      } else {
        Serial.printf("[MQTT] Failed (rc=%d), retry in 3s\n", _client.state());
        delay(3000);
      }
      attempts++;
    }
  }

  void _onMessage(char* topic, byte* payload, unsigned int length) {
    String topicStr(topic);

    // Si es audio binario, lo pasamos directo al DAC/I2S sin imprimir
    if (topicStr == TOPIC_AUDIO_PLAYBACK) {
      if (_audioPlayback != nullptr) {
        _audioPlayback->reproducirAudioMQTT(payload, length);
      }
      return;
    }

    String msg((char*)payload, length);
    Serial.printf("[MQTT] Received [%s]: %s\n", topic, msg.c_str());

    if (topicStr == TOPIC_UNLOCK_CMD && msg == "UNLOCK") {
      Serial.println("[DOOR] Unlock command received from edge service.");
      // Stub: in production, toggle GPIO relay here.
      // digitalWrite(RELAY_PIN, HIGH); delay(3000); digitalWrite(RELAY_PIN, LOW);
    }

    // Camera video streaming commands: START_VIDEO or STOP_VIDEO
    if (topicStr == TOPIC_CAMERA_TRIGGER) {
      if (msg == "START_VIDEO") {
        Serial.println("[MQTT] Starting video stream...");
        if (_camera != nullptr) _camera->startStream();
      } else if (msg == "STOP_VIDEO") {
        Serial.println("[MQTT] Stopping video stream...");
        if (_camera != nullptr) _camera->stopStream();
      }
    }

    // Live microphone streaming toggle: START or STOP
    if (topicStr == TOPIC_AUDIO_START) {
      if (msg == "START") {
        Serial.println("[MQTT] Starting live audio stream...");
        if (_audioCapture != nullptr) _audioCapture->startStreaming();
      } else if (msg == "STOP") {
        Serial.println("[MQTT] Stopping live audio stream...");
        if (_audioCapture != nullptr) _audioCapture->stopStreaming();
      }
    }
  }
};
