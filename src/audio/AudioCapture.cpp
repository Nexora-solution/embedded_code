#include "AudioCapture.h"
#include "../mqtt/MqttGateway.h"

void AudioCapture::poll(MqttGateway& mqtt) {
  if (!_streaming) return;

  // Drenamos TODO el audio disponible en el DMA en cada vuelta del loop.
  // Leer un solo bloque por ciclo (~50 ms) se quedaba atrás del ritmo de
  // captura (16 kHz) y dejaba huecos en el audio IoT→PC. Aquí publicamos
  // cada bloque disponible: la primera lectura espera un poco (por si el
  // DMA aún no llenó un bloque) y las siguientes son no bloqueantes para
  // no congelar el loop principal cuando ya no hay más datos.
  // Lecturas NO bloqueantes (timeout 0): el loop ahora corre cada ~5 ms, así
  // que drenamos lo que el DMA tenga listo sin quedarnos esperando. Antes la
  // primera lectura bloqueaba 20 ms y frenaba el procesamiento de MQTT.
  for (int i = 0; i < AUDIO_MAX_BLOCKS_PER_POLL; i++) {
    size_t bytesRead = 0;
    esp_err_t err = i2s_read(I2S_PORT, _rawBuf, sizeof(_rawBuf), &bytesRead, 0);
    if (err != ESP_OK || bytesRead == 0) break;

    // Solo contamos lo que SÍ se logró publicar. Si la conexión MQTT está
    // saturada, publishBytes devuelve false y el chunk no llega al edge —
    // contar bytesRead a secas haría creer que todo salió cuando no fue así.
    bool ok = mqtt.publishBytes(TOPIC_AUDIO_CHUNK, _rawBuf, bytesRead);
    if (ok) _bytesThisSecond += bytesRead;
  }

  unsigned long now = millis();
  if (now - _lastStatLog >= 1000) {
    Serial.printf("[INMP441] Captura: %u bytes/s publicados OK al broker.\n",
                  (unsigned)_bytesThisSecond);
    _bytesThisSecond = 0;
    _lastStatLog = now;
  }
}
