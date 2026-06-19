#include "CameraCapture.h"
#include "../mqtt/MqttGateway.h"

void CameraCapture::capture(MqttGateway& mqtt, unsigned long visitId) {
  if (!_ready) {
    Serial.println("[OV2640] Camera not ready — skipping capture.");
    return;
  }

  // ── 1. Grab a JPEG frame (allocated in PSRAM) ─────────────────────
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("[OV2640] Frame buffer capture failed.");
    return;
  }

  Serial.printf("[OV2640] Captured %u bytes JPEG (visitId=%lu)\n", fb->len, visitId);

  // ── 2. Encode to Base64 in PSRAM-backed heap ──────────────────────
  String b64 = _base64Encode(fb->buf, fb->len);

  // ── 3. Return frame buffer to the driver IMMEDIATELY ─────────────
  esp_camera_fb_return(fb);
  fb = nullptr;

  if (b64.isEmpty()) {
    Serial.println("[OV2640] Base64 encoding returned empty string — skipping publish.");
    return;
  }

  // ── 4. Build JSON envelope ────────────────────────────────────────
  String payload;
  payload.reserve(b64.length() + 48);   // 48 bytes for the JSON skeleton
  payload  = "{\"visitId\":";
  payload += String(visitId);
  payload += ",\"image\":\"";
  payload += b64;
  payload += "\"}";

  // ── 5. Publish ────────────────────────────────────────────────────
  bool ok = mqtt.publish(TOPIC_CAMERA, payload.c_str());
  if (ok) {
    Serial.printf("[OV2640] Photo published for visitId=%lu (%u chars)\n",
                  visitId, payload.length());
  } else {
    Serial.printf("[OV2640] Publish failed — packet too large? (%u chars)\n",
                  payload.length());
  }
}

const char CameraCapture::B64_CHARS[] =
  "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
