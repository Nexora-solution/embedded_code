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
    // A timeout/no-echo reading (-1) can mean two different things: an
    // object too close for the sensor's minimum range (~2cm), OR genuinely
    // nothing in range to reflect the ping. We can't tell which from a
    // single reading, so we don't special-case it here — it's treated as
    // "not detected" like any out-of-threshold distance, and the debounce
    // below absorbs brief, noisy timeouts without flipping the state.
    bool detected = (distance > 0 && distance < PRESENCE_THRESHOLD_CM);

    // Debounce: require 4 consecutive readings agreeing on the new state
    // (~2s at the current poll rate) before flipping. This is long enough
    // to ignore a stray timeout while a hand is held close, but short
    // enough to still react promptly to a real arrival/departure.
    if (detected == _pendingDetected) {
      if (_pendingCount < 255) _pendingCount++;
    } else {
      _pendingDetected = detected;
      _pendingCount = 1;
    }

    bool stateChanged = (_pendingCount >= 2 && detected != _lastDetected);

    // Heartbeat: while presence stays detected without changing, the firmware
    // would otherwise go quiet — but the Edge service has a 20s safety
    // timeout that auto-stops the stream if it doesn't hear from us. Re-send
    // "1" periodically so a hand held steady never triggers a false stop.
    bool heartbeatDue = (detected && _lastDetected &&
                          (now - _lastPublishAt >= PRESENCE_HEARTBEAT_MS));

    if (stateChanged || heartbeatDue) {
      _lastDetected  = detected;
      _lastPublishAt = now;
      const char* payload = detected ? "1" : "0";
      mqtt.publish(TOPIC_PRESENCE, payload);
      Serial.printf("[HC-SR04] Presence %s (%.1f cm)%s\n",
                    detected ? "DETECTED" : "CLEAR", distance,
                    (!stateChanged && heartbeatDue) ? " [heartbeat]" : "");
    }
  }

private:
  unsigned long _lastPoll      = 0;
  unsigned long _lastPublishAt = 0;
  bool          _lastDetected = false;
  bool          _pendingDetected = false;
  uint8_t       _pendingCount    = 0;

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
