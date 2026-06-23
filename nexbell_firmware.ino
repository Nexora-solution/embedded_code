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
#include "src/sensors/VibrationSensor.h"
#include "src/audio/AudioSystem.h"
#include "src/audio/AudioCapture.h"
#include "src/audio/AudioPlayback.h"
#include "src/camera/CameraCapture.h"
#include "src/camera/VideoMqttClient.h"

// Instantiate modules
WifiManager    wifiManager;
MqttGateway    mqttGateway;       // commands, audio, sensors (Core 1)
VideoMqttClient videoMqttClient;  // dedicated connection for video only (Core 0)
PresenceSensor presenceSensor;
DoorSensor     doorSensor;
VibrationSensor vibrationSensor;
AudioCapture   audioCapture;
AudioPlayback  audioPlayback;
CameraCapture  cameraCapture;

// Handle for the camera task
TaskHandle_t TareaCamaraHandle;

void setup() {
  Serial.begin(115200);
 
  // SECUESTRO TEMPRANO: Inicializar audio para silenciar estática al instante
  AudioSystem::inicializarAudioSilencioso();

  delay(2500);
   while (!Serial) {
    delay(10); 
  }
  Serial.println("[NexBell] Booting...");

  wifiManager.begin(WIFI_SSID, WIFI_PASSWORD);
  mqttGateway.begin();
  presenceSensor.begin();
  doorSensor.begin();         // attaches interrupt internally
  vibrationSensor.begin();
  audioCapture.begin();
  audioPlayback.begin();
  
  // Initialize the dedicated video connection and the camera
  videoMqttClient.begin();
  cameraCapture.begin();
  cameraCapture.setVideoClient(&videoMqttClient);


  mqttGateway.setCamera(&cameraCapture); // register for capture trigger
  mqttGateway.setAudioPlayback(&audioPlayback); // register for audio playback
  mqttGateway.setAudioCapture(&audioCapture); // register for live mic start/stop

  Serial.println("🚀 [Sistema] Lanzando el Mundo 2 (Video Stream) en Core 0...");

  // LA SEPARACIÓN DE NÚCLEOS (Magia FreeRTOS)
  xTaskCreatePinnedToCore(
    CameraCapture::taskCore0,    // La función que contiene la tarea
    "Servidor_Video",            // Nombre de la tarea (para debug)
    10000,                       // Tamaño de la pila (Stack en bytes)
    &cameraCapture,              // Parámetros a pasar a la tarea
    1,                           // Prioridad
    &TareaCamaraHandle,          // Puntero para rastrear la tarea
    0                            // ¡Anclado al Core 0!
  );

  Serial.print("📡 [Sistema] Setup terminado. Loop principal corriendo en el Core: ");
  Serial.println(xPortGetCoreID()); 
}

void loop() {
  wifiManager.maintain();    // non-blocking reconnect if needed
  mqttGateway.maintain();    // keep MQTT alive / process inbound commands

  presenceSensor.poll(mqttGateway);  // HC-SR04 distance poll
  doorSensor.poll(mqttGateway);      // MC38 interrupt drain and publish
  vibrationSensor.poll(mqttGateway); // Vibration sensor polling
  audioCapture.poll(mqttGateway);    // flush audio chunks when a visit is active

  vTaskDelay(pdMS_TO_TICKS(50));
}
