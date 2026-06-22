#include "CameraCapture.h"
#include "../mqtt/MqttGateway.h"

void CameraCapture::begin() {
  camera_config_t config;

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
  config.pin_sccb_sda = CAM_PIN_SIOD;
  config.pin_sccb_scl = CAM_PIN_SIOC;
  config.pin_pwdn     = CAM_PIN_PWDN;
  config.pin_reset    = CAM_PIN_RESET;

  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if (psramFound()) {
    config.frame_size   = FRAMESIZE_QVGA; // 320x240 for fluid streaming
    config.jpeg_quality = 12;             // Good compression
    config.fb_count     = 2;              // Double buffer for fluency
    config.fb_location  = CAMERA_FB_IN_PSRAM;
  } else {
    config.frame_size   = FRAMESIZE_QQVGA;
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

void CameraCapture::taskCore0(void *parameter) {
  CameraCapture* instance = static_cast<CameraCapture*>(parameter);
  Serial.print("🎥 [Core 0] Video Stream Task started on Core: ");
  Serial.println(xPortGetCoreID());

  for(;;) {
    instance->loop();
    // Allow RTOS scheduler to breathe
    vTaskDelay(pdMS_TO_TICKS(50)); // Cap at ~20 FPS max (50ms)
  }
}

void CameraCapture::loop() {
  if (!_ready || !_streaming || !_mqtt || !_mqtt->isConnected()) {
    return;
  }

  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("[OV2640] Frame buffer capture failed.");
    return;
  }

  // Publish RAW JPEG Bytes to MQTT
  bool ok = _mqtt->publishBytes(TOPIC_CAMERA_VIDEO, fb->buf, fb->len);
  
  esp_camera_fb_return(fb);

  if (!ok) {
    Serial.println("[OV2640] Failed to publish video frame over MQTT.");
  }
}
