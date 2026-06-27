#include "AudioCapture.h"
#include "AudioUdpTransport.h"

void AudioCapture::poll(AudioUdpTransport& udp) {
  if (!_streaming) return;

  // Drenamos TODO el audio disponible en el DMA en cada vuelta del loop (que
  // corre ~cada 5 ms) con lecturas NO bloqueantes. Cada bloque del micrófono
  // se manda DIRECTO por UDP al Edge — ya no pasa por el broker MQTT, lo que
  // baja la latencia y libera el WiFi/MQTT para el video.
  for (int i = 0; i < AUDIO_MAX_BLOCKS_PER_POLL; i++) {
    size_t bytesRead = 0;
    esp_err_t err = i2s_read(I2S_PORT, _rawBuf, sizeof(_rawBuf), &bytesRead, 0);
    if (err != ESP_OK || bytesRead == 0) break;

    bool ok = udp.sendMic(_rawBuf, bytesRead);
    if (ok) _bytesThisSecond += bytesRead;
  }

  unsigned long now = millis();
  if (now - _lastStatLog >= 1000) {
    Serial.printf("[INMP441] Captura: %u bytes/s enviados por UDP al Edge.\n",
                  (unsigned)_bytesThisSecond);
    _bytesThisSecond = 0;
    _lastStatLog = now;
  }
}
