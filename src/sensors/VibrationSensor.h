#pragma once
#include <Arduino.h>
#include "../config/Config.h"
#include "../mqtt/MqttGateway.h"

/**
 * Vibration/Tamper Sensor
 * Polls the vibration sensor (SW-420 or similar). When vibration is detected,
 * it publishes an alarm to `nexbell/alarms/vibration`. Includes a cooldown
 * to prevent flooding the MQTT broker with continuous alarms.
 */
class VibrationSensor {
public:
  void begin() {
    pinMode(VIBRATION_SENSOR_PIN, INPUT);
    Serial.println("[Vibration] Vibration sensor ready.");
  }

  void poll(MqttGateway& mqtt) {
    unsigned long now = millis();
    
    // Read the sensor (assuming HIGH means vibration detected)
    int state = digitalRead(VIBRATION_SENSOR_PIN);
    
    // If vibration is detected and cooldown has passed
    if (state == HIGH && (now - _lastAlarmTime > COOLDOWN_MS)) {
      _lastAlarmTime = now;
      Serial.println("[Vibration] TAMPERING DETECTED! Publishing alarm...");
      mqtt.publish(TOPIC_VIBRATION_ALARM, "VIBRATION_DETECTED");
    }
  }

private:
  unsigned long _lastAlarmTime = 0;
  const unsigned long COOLDOWN_MS = 5000; // Wait 5 seconds before sending another alarm
};
