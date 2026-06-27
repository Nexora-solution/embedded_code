#pragma once
#include <Arduino.h>
#include <driver/i2s.h>
#include "../config/Config.h"

/**
 * AudioPlayback
 * 
 * Se encarga de recibir fragmentos de audio (vía MQTT) e inyectarlos
 * directamente en el puerto I2S para reproducirlos por el altavoz MAX98357A.
 * Gracias a `tx_desc_auto_clear = true`, no necesita manejar el silencio manual.
 */
class AudioPlayback {
public:
  void begin() {
    // La inicialización del hardware ya fue hecha por AudioSystem
    // Aquí podríamos inicializar variables de estado si fuesen necesarias
    Serial.println("[AudioPlayback] Módulo de reproducción listo.");
  }

  /**
   * Esta función se llama desde MqttGateway cuando llega un payload
   * en el tópico TOPIC_AUDIO_PLAYBACK.
   */
  void reproducirAudioMQTT(uint8_t* payload, size_t longitud) {
    size_t bytes_escritos;

    // Esta llamada ocurre DENTRO del callback de MQTT, que ya tiene el mutex
    // de la conexión tomado (ver MqttGateway::maintain()). Si esperáramos
    // indefinidamente (portMAX_DELAY) a que el DMA tenga espacio, podríamos
    // bloquear la cámara y el micrófono por mucho tiempo cuando el sistema
    // está saturado. Con un timeout corto, si no hay espacio a tiempo
    // simplemente se descarta este fragmento y seguimos — preferible perder
    // un trozo de audio que congelar todo el sistema.
    // Timeout de 30 ms: damos tiempo a que el buffer del altavoz acepte el
    // fragmento en vez de descartarlo. Priorizamos fluidez (sin cortes) sobre
    // latencia mínima.
    esp_err_t err = i2s_write(I2S_PORT, payload, longitud, &bytes_escritos, pdMS_TO_TICKS(30));

    if (err != ESP_OK) {
      Serial.printf("[AudioPlayback] Error al escribir I2S: %d\n", err);
    } else if (bytes_escritos < longitud) {
      Serial.printf("[AudioPlayback] Buffer lleno — se descartaron %u de %u bytes.\n",
                    (unsigned)(longitud - bytes_escritos), (unsigned)longitud);
    }

    // Diagnóstico (1 vez/seg): confirma si el ESP32 está RECIBIENDO y
    // reproduciendo el audio de la PC. Si ves "> 0 bytes/s" aquí mientras
    // hablas a tu micrófono de la PC, el audio SÍ llega al parlante (sería
    // solo cuestión de volumen). Si ves "0 bytes/s", el audio no está
    // llegando: el problema está en la web o el edge, no en el parlante.
    _bytesPlayedThisSecond += bytes_escritos;
    unsigned long now = millis();
    if (now - _lastPlayLog >= 1000) {
      Serial.printf("[AudioPlayback] Reproduccion: %u bytes/s recibidos de la PC.\n",
                    (unsigned)_bytesPlayedThisSecond);
      _bytesPlayedThisSecond = 0;
      _lastPlayLog = now;
    }
  }

private:
  unsigned long _bytesPlayedThisSecond = 0;
  unsigned long _lastPlayLog           = 0;
};
