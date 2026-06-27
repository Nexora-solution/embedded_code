#pragma once
#include <Arduino.h>
#include <driver/i2s.h>
#include "../config/Config.h"

class AudioUdpTransport;

/**
 * INMP441 I2S Microphone — Live Audio Capture
 *
 * Streams raw 16kHz/16-bit PCM audio continuously while active, the same
 * way CameraCapture streams video: started/stopped by a command, with no
 * fixed duration. Each DMA buffer read is published directly as raw
 * binary bytes to `nexbell/audio/chunk` — no Base64, no JSON envelope —
 * to keep latency and CPU overhead low for a live conversation.
 */
class AudioCapture {
public:
  void begin() {
    // El hardware I2S ya fue inicializado por AudioSystem en modo Full-Duplex.
    Serial.println("[INMP441] AudioCapture listo (usando I2S compartido).");
  }

  void startStreaming() {
    _streaming = true;
    Serial.println("[INMP441] Live audio streaming started.");
  }

  void stopStreaming() {
    _streaming = false;
    Serial.println("[INMP441] Live audio streaming stopped.");
  }

  bool isStreaming() const { return _streaming; }

  /** Called from the main loop. While streaming, reads the mic DMA and sends it over UDP. */
  void poll(AudioUdpTransport& udp);

private:
  bool          _streaming = false;
  uint8_t       _rawBuf[I2S_BUFFER_LEN * 2]; // 16-bit samples
  unsigned long _bytesThisSecond = 0;        // diagnóstico: bytes publicados en el último segundo
  unsigned long _lastStatLog     = 0;        // marca de tiempo del último log de diagnóstico
};
