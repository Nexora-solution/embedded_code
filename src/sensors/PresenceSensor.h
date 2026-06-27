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

    // Un -1 (sin eco) es ambiguo: puede ser un objeto DEMASIADO CERCA (dentro
    // de la zona muerta de ~2cm del HC-SR04) o que no haya nada. Antes esto se
    // tomaba como "no detectado" y apagaba la cámara aunque la persona siguiera
    // ahí (mano pegada al sensor). Ahora, ante un -1, MANTENEMOS el estado
    // actual en vez de voltearlo — así una mano pegada no apaga el stream.
    bool detected;
    if (distance < 0) {
      detected = _lastDetected;                       // sin eco → conservar estado
    } else {
      detected = (distance < PRESENCE_THRESHOLD_CM);  // lectura válida
    }

    // Debounce ASIMÉTRICO: rápido para PRENDER, lento para APAGAR. Encender con
    // 2 lecturas (~1s) reacciona pronto a una llegada; apagar exige 6 lecturas
    // (~3s) seguidas de "vacío" para no cortar la cámara por un bache momentáneo.
    if (detected == _pendingDetected) {
      if (_pendingCount < 255) _pendingCount++;
    } else {
      _pendingDetected = detected;
      _pendingCount = 1;
    }

    uint8_t needed = detected ? PRESENCE_ON_READINGS : PRESENCE_OFF_READINGS;
    bool stateChanged = (_pendingCount >= needed && detected != _lastDetected);

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
