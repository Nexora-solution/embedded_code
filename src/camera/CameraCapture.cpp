#include "CameraCapture.h"
#include "VideoTcpClient.h"
#include "FaceAI.h"
#include "../audio/AudioCapture.h"

// Registrar implica encender la IA (si no, no habría con qué capturar la cara).
void CameraCapture::enrollFace()  { _faceActive = true; if (_face) _face->enrollNext(); }
void CameraCapture::deleteFaces() { if (_face) _face->deleteAll(); }

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
    // Calidad 16 (antes 12): comprime un poco más → frames ~40% más livianos.
    // Con el audio compartiendo el mismo WiFi, frames más chicos dejan ancho
    // de banda para que el audio no haga estática y el video no se congele.
    config.jpeg_quality = 16;
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
    // FPS adaptativo: el ESP32 tiene UN solo WiFi. Si el audio en vivo está
    // activo, bajamos a ~10 FPS (100ms) para dejarle ancho de banda y que el
    // envío TCP del video no se quede bloqueado (eso apagaba el video al
    // hablar). Sin audio, vamos a ~20 FPS (50ms) para máxima fluidez.
    bool audioActive = (instance->_audio != nullptr && instance->_audio->isStreaming());
    vTaskDelay(pdMS_TO_TICKS(audioActive ? 100 : 50));
  }
}

void CameraCapture::loop() {
  // Keep the dedicated video connection alive (reconnect if dropped, send
  // keepalive pings) on every iteration, regardless of streaming state.
  if (_mqtt) _mqtt->maintain();

  // La cámara transmite SOLO con presencia (_streaming). Al quitar la presencia
  // se apaga. El reconocimiento facial igual funciona: la persona a registrar o
  // reconocer ya está frente a la cámara, así que la presencia ya está activa.
  // (La IA solo corre cuando además el reconocimiento está ON — ver el bloque
  //  FACE_DETECT más abajo, que comprueba _faceActive.)
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
  size_t frameLen = fb->len;

#if FACE_DETECT_ENABLED
  // Correr la IA SOLO si está activada (al trabajar con residentes). En el flujo
  // normal de visitantes queda apagada → el video va a su fluidez completa.
  // Corre 1 de cada N frames, después de enviar el frame y antes de liberar fb.
  _frameCount++;
  if (_faceActive && _face && (_frameCount % FACE_DETECT_EVERY_N_FRAMES == 0)) {
    _face->process(fb);
  }
#endif

  esp_camera_fb_return(fb);

  if (!ok) {
    // Rate-limited: avoid flooding Serial (and corrupting output shared with Core 1 logs)
    // on every dropped frame — just report once per second how many were lost.
    _droppedFrames++;
    unsigned long now = millis();
    if (now - _lastDropLog >= 1000) {
      Serial.printf("[OV2640] Dropped %u frame(s) in the last second (last size: %u bytes — MQTT/WiFi congestion, not a buffer size issue)\n",
                    _droppedFrames, (unsigned)frameLen);
      _droppedFrames = 0;
      _lastDropLog = now;
    }
  }
}
