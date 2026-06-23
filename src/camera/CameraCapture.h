#pragma once
#include <Arduino.h>
#include <esp_camera.h>
#include "../config/Config.h"

class VideoMqttClient;

#define TOPIC_CAMERA_VIDEO "nexbell/telemetry/video"

/**
 * OV2640 Camera Capture Module (FreeRTOS Core 0)
 *
 * Takes JPEGs continuously and pushes them as pure binary bytes over its
 * own dedicated MQTT connection (VideoMqttClient) — separate from the
 * connection used for commands/audio/sensors, so video never has to wait
 * its turn behind anything else.
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

  // Set the dedicated video MQTT client reference so the task can use it
  void setVideoClient(VideoMqttClient* client) { _mqtt = client; }

private:
  bool _ready = false;
  bool _streaming = false;
  VideoMqttClient* _mqtt = nullptr;
  unsigned int  _droppedFrames = 0;
  unsigned long _lastDropLog   = 0;

  void loop();
};
