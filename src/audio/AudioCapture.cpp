#include "AudioCapture.h"
#include "../mqtt/MqttGateway.h"

void AudioCapture::poll(MqttGateway& mqtt) {
  if (!_streaming) return;

  // Timeout corto en vez de portMAX_DELAY: este poll() corre en el loop
  // principal junto a los demás sensores y el mantenimiento de MQTT — no
  // debe quedarse esperando al DMA si no hay datos listos todavía.
  size_t bytesRead = 0;
  esp_err_t err = i2s_read(I2S_PORT, _rawBuf, sizeof(_rawBuf), &bytesRead, pdMS_TO_TICKS(30));
  if (err != ESP_OK || bytesRead == 0) return;

  mqtt.publishBytes(TOPIC_AUDIO_CHUNK, _rawBuf, bytesRead);
}
