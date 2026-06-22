#pragma once
#include <Arduino.h>
#include <driver/i2s.h>
#include "../config/Config.h"
#include "../mqtt/MqttGateway.h"

// Topic for raw audio chunks sent to the edge service
#define TOPIC_AUDIO_CHUNK  "nexbell/audio/chunk"
// Topic to signal the edge service that recording is complete
#define TOPIC_AUDIO_DONE   "nexbell/audio/done"
// Topic the edge service publishes to to trigger recording on the device
#define TOPIC_AUDIO_START  "nexbell/audio/start"

/**
 * INMP441 I2S Microphone Audio Capture
 *
 * Flow:
 *   1. Edge service publishes to `nexbell/audio/start` with the visitRequestId.
 *   2. AudioCapture starts DMA recording for AUDIO_RECORD_SECONDS.
 *   3. Each filled DMA buffer is chunked and published to `nexbell/audio/chunk`
 *      with a JSON envelope: { "id": <visitId>, "seq": <n>, "data": <base64> }
 *   4. When done, publishes an empty payload to `nexbell/audio/done` so the
 *      edge service knows to assemble and upload the full file.
 */
class AudioCapture {
public:
  void begin() {
    // La inicialización del hardware ya fue hecha por AudioSystem en modo Full-Duplex
    Serial.println("[INMP441] AudioCapture listo (usando I2S compartido).");
  }

  /**
   * Called from main loop. When recording is active, reads DMA buffers and
   * publishes chunks. When recording window ends, publishes the done signal.
   */
  void poll(MqttGateway& mqtt) {
    if (!_recording) return;

    if (millis() - _recordStart >= (unsigned long)AUDIO_RECORD_SECONDS * 1000UL) {
      // Recording window elapsed – signal edge service
      _recording = false;
      char donePayload[32];
      snprintf(donePayload, sizeof(donePayload), "{\"id\":%lu}", _visitId);
      mqtt.publish(TOPIC_AUDIO_DONE, donePayload);
      Serial.printf("[INMP441] Recording complete for visitId=%lu\n", _visitId);
      return;
    }

    // Read one DMA buffer
    size_t bytesRead = 0;
    esp_err_t err = i2s_read(I2S_PORT, _rawBuf, sizeof(_rawBuf), &bytesRead, portMAX_DELAY);
    if (err != ESP_OK || bytesRead == 0) return;

    // Simple Base64 encoding for binary-safe MQTT transport
    String b64 = _base64Encode(_rawBuf, bytesRead);

    // Build chunk JSON envelope
    String envelope = "{\"id\":" + String(_visitId) +
                      ",\"seq\":" + String(_seq++) +
                      ",\"data\":\"" + b64 + "\"}"; // keep under 128KB MQTT limit

    mqtt.publish(TOPIC_AUDIO_CHUNK, envelope.c_str());
  }

  /** Called by MqttGateway when `nexbell/audio/start` arrives. Payload = visitRequestId. */
  void startRecording(unsigned long visitId) {
    _visitId     = visitId;
    _seq         = 0;
    _recordStart = millis();
    _recording   = true;
    Serial.printf("[INMP441] Started recording for visitId=%lu\n", visitId);
  }

private:
  bool          _recording   = false;
  unsigned long _visitId     = 0;
  uint16_t      _seq         = 0;
  unsigned long _recordStart = 0;
  uint8_t       _rawBuf[I2S_BUFFER_LEN * 2]; // 16-bit samples

  // ── Minimal Base64 encoder (no external lib required) ────────────
  static const char B64_CHARS[];

  String _base64Encode(const uint8_t* data, size_t len) {
    String out;
    out.reserve(((len + 2) / 3) * 4 + 1);
    for (size_t i = 0; i < len; i += 3) {
      uint32_t b = ((uint32_t)data[i]) << 16;
      if (i + 1 < len) b |= ((uint32_t)data[i + 1]) << 8;
      if (i + 2 < len) b |= data[i + 2];
      out += B64_CHARS[(b >> 18) & 0x3F];
      out += B64_CHARS[(b >> 12) & 0x3F];
      out += (i + 1 < len) ? B64_CHARS[(b >> 6) & 0x3F] : '=';
      out += (i + 2 < len) ? B64_CHARS[b & 0x3F]         : '=';
    }
    return out;
  }
};

const char AudioCapture::B64_CHARS[] =
  "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
