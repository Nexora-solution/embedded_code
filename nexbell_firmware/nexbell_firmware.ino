/**
 * NexBell ESP32S3 Firmware
 * Main entry point - initialises all hardware modules and the MQTT gateway.
 * Board: ESP32S3
 * Framework: Arduino
 */

#include "src/config/Config.h"
#include "src/wifi/WifiManager.h"
#include "src/mqtt/MqttGateway.h"
#include "src/sensors/PresenceSensor.h"
#include "src/sensors/DoorSensor.h"
#include "src/audio/AudioCapture.h"

// Instantiate modules
WifiManager  wifiManager;
MqttGateway  mqttGateway;
PresenceSensor presenceSensor;
DoorSensor   doorSensor;
AudioCapture  audioCapture;

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("[NexBell] Booting...");

  wifiManager.begin(WIFI_SSID, WIFI_PASSWORD);
  mqttGateway.begin();
  presenceSensor.begin();
  doorSensor.begin();         // attaches interrupt internally
  audioCapture.begin();

  Serial.println("[NexBell] Boot complete.");
}

void loop() {
  wifiManager.maintain();    // non-blocking reconnect if needed
  mqttGateway.maintain();    // keep MQTT alive / process inbound commands

  presenceSensor.poll(mqttGateway);  // HC-SR04 distance poll
  doorSensor.poll(mqttGateway);      // MC38 interrupt drain and publish
  audioCapture.poll(mqttGateway);    // flush audio chunks when a visit is active

  delay(50);
}
