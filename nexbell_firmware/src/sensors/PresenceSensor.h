#pragma once
#include <Arduino.h>
#include "../config/Config.h"
#include "../mqtt/MqttGateway.h"

/**
 * HC-SR04 Ultrasonic Presence Sensor
 * Polls the sensor at a configurable interval and publishes presence state
 * to `nexbell/telemetry/presence` when state changes.
 */
class PresenceSensor {
public:
  void begin() {
    pinMode(HC_TRIG_PIN, OUTPUT);
    pinMode(HC_ECHO_PIN, INPUT);
    Serial.println("[HC-SR04] Presence sensor ready.");
  }

  void poll(MqttGateway& mqtt) {
    unsigned long now = millis();
    if (now - _lastPoll < PRESENCE_POLL_MS) return;
    _lastPoll = now;

    float distance = _measureCm();
    bool detected  = (distance > 0 && distance < PRESENCE_THRESHOLD_CM);

    // Only publish on state change to avoid flooding the broker.
    if (detected != _lastDetected) {
      _lastDetected = detected;
      const char* payload = detected ? "1" : "0";
      mqtt.publish(TOPIC_PRESENCE, payload);
      Serial.printf("[HC-SR04] Presence %s (%.1f cm)\n",
                    detected ? "DETECTED" : "CLEAR", distance);
    }
  }

private:
  unsigned long _lastPoll    = 0;
  bool          _lastDetected = false;

  float _measureCm() {
    // Send 10µs trigger pulse
    digitalWrite(HC_TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(HC_TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(HC_TRIG_PIN, LOW);

    // Measure echo duration (timeout 30ms = ~5m max range)
    long duration = pulseIn(HC_ECHO_PIN, HIGH, 30000);
    if (duration == 0) return -1;   // timeout / no echo
    return duration * 0.0343f / 2.0f;
  }
};
