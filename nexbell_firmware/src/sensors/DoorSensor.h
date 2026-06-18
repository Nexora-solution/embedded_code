#pragma once
#include <Arduino.h>
#include "../config/Config.h"
#include "../mqtt/MqttGateway.h"

// Forward declaration – ISR needs a global pointer to publish from the interrupt context.
static volatile bool g_doorChanged = false;
static volatile bool g_doorOpen    = false;

void IRAM_ATTR doorISR() {
  g_doorOpen    = (digitalRead(MC38_PIN) == HIGH); // HIGH = open (NO contact broken)
  g_doorChanged = true;
}

/**
 * MC38 Magnetic Door Sensor
 * Uses a hardware GPIO interrupt so state changes are captured instantly
 * without polling overhead. The ISR sets a flag; the main loop publishes.
 */
class DoorSensor {
public:
  void begin() {
    // MC38 output is typically HIGH (open) when the magnet is removed.
    // Use INPUT_PULLDOWN to ensure a stable LOW when the door is closed.
    pinMode(MC38_PIN, INPUT_PULLDOWN);
    attachInterrupt(digitalPinToInterrupt(MC38_PIN), doorISR, CHANGE);
    Serial.println("[MC38] Door sensor ISR attached.");
  }

  /**
   * Call in main loop – drains the ISR flag and publishes the state via MQTT.
   * Kept separate from the ISR to allow safe use of String / PubSubClient.
   */
  void poll(MqttGateway& mqtt) {
    if (!g_doorChanged) return;
    g_doorChanged = false;  // reset before reading to avoid race

    const char* state = g_doorOpen ? "OPEN" : "CLOSED";
    mqtt.publish(TOPIC_DOOR_ALARM, state);
    Serial.printf("[MC38] Door state changed: %s\n", state);
  }
};
