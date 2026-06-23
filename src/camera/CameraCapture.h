#pragma once
#include <Arduino.h>
#include <esp_camera.h>
#include "../config/Config.h"

class MqttGateway;

#define TOPIC_CAMERA_VIDEO "nexbell/telemetry/video"

/**
 * OV2640 Camera Capture Module (FreeRTOS Core 0)
 * 
 * Takes JPEGs continuously and pushes them to MQTT as pure binary bytes.
 */
class CameraCapture {
public:
  void begin();
  
  // FreeRTOS task entry point
  static void taskCore0(void *parameter);

  void startStream() { _streaming = true; }
  void stopStream()  { _streaming = false; }
  bool isStreaming() const { return _streaming; }
  bool isReady() const { return _ready; }

  // Set the MQTT gateway reference so the task can use it
  void setMqttGateway(MqttGateway* mqtt) { _mqtt = mqtt; }

private:
  bool _ready = false;
  bool _streaming = false;
  MqttGateway* _mqtt = nullptr;
  unsigned int  _droppedFrames = 0;
  unsigned long _lastDropLog   = 0;

  void loop();
};
