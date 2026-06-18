#pragma once
#include <PubSubClient.h>
#include <WiFi.h>
#include "../config/Config.h"

/**
 * Thin wrapper over PubSubClient.
 * Handles connection, subscriptions, and publish helpers.
 * Processes inbound UNLOCK commands and drives the door relay signal
 * (relay/actuator code is excluded per spec – the handler is stubbed for
 * future hardware integration).
 */
class MqttGateway {
public:
  void begin() {
    _client.setClient(_wifiClient);
    _client.setServer(MQTT_BROKER_HOST, MQTT_BROKER_PORT);
    _client.setCallback([this](char* topic, byte* payload, unsigned int length) {
      _onMessage(topic, payload, length);
    });
    _reconnect();
  }

  /** Keep connection alive, process inbound messages. */
  void maintain() {
    if (!_client.connected()) {
      _reconnect();
    }
    _client.loop();
  }

  /** Publish a string payload to a topic. */
  bool publish(const char* topic, const char* payload) {
    if (!_client.connected()) return false;
    return _client.publish(topic, payload);
  }

  /** Publish raw binary bytes (for audio chunks). */
  bool publishBytes(const char* topic, const uint8_t* data, size_t len) {
    if (!_client.connected()) return false;
    return _client.publish(topic, data, len);
  }

  bool isConnected() const { return _client.connected(); }

private:
  WiFiClient   _wifiClient;
  PubSubClient _client;

  void _reconnect() {
    int attempts = 0;
    while (!_client.connected() && attempts < 3) {
      Serial.printf("[MQTT] Connecting to %s:%d ...\n", MQTT_BROKER_HOST, MQTT_BROKER_PORT);
      if (_client.connect(MQTT_CLIENT_ID)) {
        Serial.println("[MQTT] Connected.");
        _client.subscribe(TOPIC_UNLOCK_CMD);
        Serial.printf("[MQTT] Subscribed to %s\n", TOPIC_UNLOCK_CMD);
      } else {
        Serial.printf("[MQTT] Failed (rc=%d), retry in 3s\n", _client.state());
        delay(3000);
      }
      attempts++;
    }
  }

  void _onMessage(char* topic, byte* payload, unsigned int length) {
    String topicStr(topic);
    String msg((char*)payload, length);
    Serial.printf("[MQTT] Received [%s]: %s\n", topic, msg.c_str());

    if (topicStr == TOPIC_UNLOCK_CMD && msg == "UNLOCK") {
      Serial.println("[DOOR] Unlock command received from edge service.");
      // Stub: in production, toggle GPIO relay here.
      // digitalWrite(RELAY_PIN, HIGH); delay(3000); digitalWrite(RELAY_PIN, LOW);
    }
  }
};
