#pragma once
#include <Arduino.h>
#include <esp_camera.h>
#include "../config/Config.h"

class MqttGateway;

// MQTT topic the ESP32S3 publishes camera frames to
#define TOPIC_CAMERA   "nexbell/telemetry/camera"

// MQTT topic the edge service publishes to trigger a photo (payload = visitId)
#define TOPIC_CAMERA_TRIGGER "nexbell/commands/capture"

/**
 * OV2640 Camera Capture Module
 *
 * Flow:
 *   1. Edge service (or backend) publishes a visitId to `nexbell/commands/capture`.
 *   2. MqttGateway forwards the visitId to CameraCapture::capture().
 *   3. CameraCapture grabs a JPEG frame from the OV2640 into PSRAM.
 *   4. The JPEG bytes are encoded to Base64 in small chunks to avoid stack overflow.
 *   5. A JSON envelope is built in PSRAM and published on `nexbell/telemetry/camera`.
 *      Payload: { "visitId": <id>, "image": "<base64 string>" }
 *   6. Edge service receives the topic, decodes Base64 to buffer, uploads to
 *      object storage, and POSTs the resulting URL to the Spring Boot backend.
 *
 * Memory strategy:
 *   - esp_camera_fb_get() allocates the JPEG frame buffer in PSRAM automatically
 *     when BOARD_HAS_PSRAM is defined and PSRAM is available.
 *   - The Base64-encoded string is built using heap-allocated String (also in PSRAM
 *     on ESP32S3 with PSRAM enabled via psramMalloc / String reserve).
 *   - The JSON envelope is assembled from sub-strings to avoid a single giant stack
 *     allocation.
 */
class CameraCapture {
public:
  void begin() {
    camera_config_t config;

    // ── Pin mapping for AI Thinker / M5Stack-compatible OV2640 module ──
    // Adjust these to match your exact wiring if you use a bare module.
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer   = LEDC_TIMER_0;
    config.pin_d0       = CAM_PIN_D0;
    config.pin_d1       = CAM_PIN_D1;
    config.pin_d2       = CAM_PIN_D2;
    config.pin_d3       = CAM_PIN_D3;
    config.pin_d4       = CAM_PIN_D4;
    config.pin_d5       = CAM_PIN_D5;
    config.pin_d6       = CAM_PIN_D6;
    config.pin_d7       = CAM_PIN_D7;
    config.pin_xclk     = CAM_PIN_XCLK;
    config.pin_pclk     = CAM_PIN_PCLK;
    config.pin_vsync    = CAM_PIN_VSYNC;
    config.pin_href     = CAM_PIN_HREF;
    config.pin_sscb_sda = CAM_PIN_SIOD;
    config.pin_sscb_scl = CAM_PIN_SIOC;
    config.pin_pwdn     = CAM_PIN_PWDN;
    config.pin_reset    = CAM_PIN_RESET;

    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;  // JPEG output — compact over MQTT

    // Use PSRAM for the frame buffer if available
    if (psramFound()) {
      // 320x240 JPEG ~ 10–30 KB; 640x480 ~ 30–80 KB.
      // Choose FRAMESIZE_QVGA (320x240) to stay within the 50 KB MQTT buffer.
      config.frame_size   = FRAMESIZE_QVGA;
      config.jpeg_quality = 12;   // 0 = best quality, 63 = worst
      config.fb_count     = 2;    // double-buffered in PSRAM
      config.fb_location  = CAMERA_FB_IN_PSRAM;
    } else {
      // Fallback for boards without PSRAM — very small frame
      config.frame_size   = FRAMESIZE_QQVGA;   // 160x120
      config.jpeg_quality = 20;
      config.fb_count     = 1;
      config.fb_location  = CAMERA_FB_IN_DRAM;
    }

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
      Serial.printf("[OV2640] Camera init failed: 0x%x\n", err);
      _ready = false;
      return;
    }

    _ready = true;
    Serial.println("[OV2640] Camera initialised.");
  }

  /**
   * Captures one JPEG frame, encodes it as Base64 and publishes it to MQTT.
   * Call this from MqttGateway's _onMessage when TOPIC_CAMERA_TRIGGER arrives.
   *
   * @param mqtt     Reference to the MQTT gateway for publishing.
   * @param visitId  The visit request ID this photo is evidence for.
   */
  void capture(MqttGateway& mqtt, unsigned long visitId);

  bool isReady() const { return _ready; }

private:
  bool _ready = false;

  // ── Minimal Base64 encoder (same implementation used in AudioCapture) ──
  // Processes the input in 3-byte groups to avoid large temporary buffers.
  static const char B64_CHARS[];

  String _base64Encode(const uint8_t* data, size_t len) {
    size_t outLen = ((len + 2) / 3) * 4;
    String out;
    // reserve() works from PSRAM when BOARD_HAS_PSRAM is defined
    if (!out.reserve(outLen + 1)) {
      Serial.println("[OV2640] Base64: reserve() failed — not enough heap.");
      return String();
    }

    for (size_t i = 0; i < len; i += 3) {
      uint32_t b  = ((uint32_t)data[i]) << 16;
      if (i + 1 < len) b |= ((uint32_t)data[i + 1]) << 8;
      if (i + 2 < len) b |= data[i + 2];

      out += B64_CHARS[(b >> 18) & 0x3F];
      out += B64_CHARS[(b >> 12) & 0x3F];
      out += (i + 1 < len) ? B64_CHARS[(b >> 6) & 0x3F] : '=';
      out += (i + 2 < len) ? B64_CHARS[ b       & 0x3F] : '=';
    }
    return out;
  }
};
