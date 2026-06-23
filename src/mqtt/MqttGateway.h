#pragma once
#include <PubSubClient.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "../config/Config.h"
#include "../camera/CameraCapture.h"
#include "../audio/AudioPlayback.h"

/**
 * Thin wrapper over PubSubClient.
 * Handles connection, subscriptions, and publish helpers.
 * Processes inbound UNLOCK commands and drives the door relay signal
 * (relay/actuator code is excluded per spec – the handler is stubbed for
 * future hardware integration).
 *
 * PubSubClient/WiFiClient are NOT thread-safe. The video stream task runs
 * pinned to Core 0 and calls publishBytes() concurrently with the main
 * loop() on Core 1 calling maintain(). A mutex serializes all access to
 * the underlying client so the two cores never touch the socket at the
 * same time (this was causing TCP write failures and reconnect loops
 * while the camera stream stayed on for more than a few seconds).
 */
class MqttGateway {
public:
  void begin() {
    _mutex = xSemaphoreCreateMutex();
    _client.setClient(_wifiClient);
    _client.setServer(MQTT_BROKER_HOST, MQTT_BROKER_PORT);
    _client.setCallback([this](char* topic, byte* payload, unsigned int length) {
      _onMessage(topic, payload, length);
    });
    _client.setBufferSize(65000); // ~64KB (máximo soportado por PubSubClient es 65535) — los frames JPEG pueden superar 32KB con escenas detalladas
    _reconnect();
  }

  /** Call after begin() to register the camera module for capture triggers. */
  void setCamera(CameraCapture* cam) { _camera = cam; }
  
  /** Call after begin() to register the audio playback module. */
  void setAudioPlayback(AudioPlayback* ap) { _audioPlayback = ap; }

  /** Keep connection alive, process inbound messages. Called from the main loop (Core 1). */
  void maintain() {
    if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(100)) != pdTRUE) return; // camera task is mid-publish, retry next cycle
    if (!_client.connected()) {
      _reconnect();
    }
    _client.loop();
    xSemaphoreGive(_mutex);
  }

  /** Publish a string payload to a topic. */
  bool publish(const char* topic, const char* payload) {
    if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(200)) != pdTRUE) return false;
    bool ok = _client.connected() && _client.publish(topic, payload);
    xSemaphoreGive(_mutex);
    return ok;
  }

  /** Publish raw binary bytes (for video frames / audio chunks). Called from the camera task (Core 0). */
  bool publishBytes(const char* topic, const uint8_t* data, size_t len) {
    if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(200)) != pdTRUE) return false;
    bool ok = _client.connected() && _client.publish(topic, data, len);
    xSemaphoreGive(_mutex);
    return ok;
  }

  bool isConnected() { return _client.connected(); }

private:
  WiFiClient    _wifiClient;
  PubSubClient  _client;
  CameraCapture* _camera = nullptr;
  AudioPlayback* _audioPlayback = nullptr;
  SemaphoreHandle_t _mutex = nullptr;

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
  }
};
