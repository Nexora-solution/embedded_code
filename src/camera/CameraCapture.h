#pragma once
#include <Arduino.h>
#include <esp_camera.h>
#include "../config/Config.h"

class VideoTcpClient;
class AudioCapture;

#define TOPIC_CAMERA_VIDEO "nexbell/telemetry/video"

/**
 * OV2640 Camera Capture Module (FreeRTOS Core 0)
 *
 * Takes JPEGs continuously and pushes them as pure binary bytes over a
 * dedicated TCP socket to the Edge (VideoTcpClient) — out of the MQTT broker,
 * so video doesn't choke when live audio is also flowing.
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

  // Set the dedicated video TCP client reference so the task can use it
  void setVideoClient(VideoTcpClient* client) { _mqtt = client; }

  // Lets the camera task know when live audio is active, so it can lower the
  // frame rate and leave WiFi bandwidth for the audio (keeps video alive).
  void setAudioMonitor(AudioCapture* audio) { _audio = audio; }

private:
  bool _ready = false;
  bool _streaming = false;
  VideoTcpClient* _mqtt = nullptr;
  AudioCapture*   _audio = nullptr;
  unsigned int  _droppedFrames = 0;
  unsigned long _lastDropLog   = 0;

  void loop();
};
