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
#include "src/audio/AudioUdpTransport.h"
#include "src/camera/CameraCapture.h"
#include "src/camera/VideoMqttClient.h"

// Instantiate modules
WifiManager    wifiManager;
MqttGateway    mqttGateway;       // commands, sensors, mic START/STOP control (Core 1)
VideoMqttClient videoMqttClient;  // dedicated connection for video only (Core 0)
PresenceSensor presenceSensor;
DoorSensor     doorSensor;
VibrationSensor vibrationSensor;
AudioCapture   audioCapture;
AudioPlayback  audioPlayback;
AudioUdpTransport audioUdp;       // live voice over UDP, direct to the Edge (not MQTT)
CameraCapture  cameraCapture;

// Buffer for incoming portero audio arriving over UDP.
static uint8_t udpPlaybackBuf[2048];

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
  audioUdp.begin();           // open the UDP socket for live voice (mic out / portero in)

  // Initialize the dedicated video connection and the camera
  videoMqttClient.begin();
  cameraCapture.begin();
  cameraCapture.setVideoClient(&videoMqttClient);


  mqttGateway.setCamera(&cameraCapture); // register for capture trigger
  mqttGateway.setAudioCapture(&audioCapture); // register for live mic start/stop (MQTT control)

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
  audioCapture.poll(audioUdp);       // send live mic to the Edge over UDP

  // Drena el audio del portero que llega por UDP y lo reproduce. Se procesan
  // todos los paquetes pendientes en cada vuelta para no acumular retraso.
  int n;
  while ((n = audioUdp.receivePlayback(udpPlaybackBuf, sizeof(udpPlaybackBuf))) > 0) {
    audioPlayback.reproducir(udpPlaybackBuf, (size_t)n);
  }

  // 5 ms de loop: mantiene el procesamiento de MQTT (comandos/control) ágil y
  // drena el audio UDP entrante a tiempo. Los sensores tienen su propio
  // intervalo interno, así que no se afectan.
  vTaskDelay(pdMS_TO_TICKS(5));
}
