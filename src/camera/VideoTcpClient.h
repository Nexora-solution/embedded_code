#pragma once
#include <WiFi.h>
#include "../config/Config.h"

/**
 * VideoTcpClient — envía los frames de video por un socket TCP directo al Edge
 * (ya NO por el broker MQTT). Cada frame se manda como:
 *
 *     [ 4 bytes: longitud (big-endian) ][ bytes JPEG ]
 *
 * Así el Edge sabe exactamente dónde empieza y termina cada frame. TCP es
 * ideal para frames grandes (5-15KB): los entrega completos y en orden, sin
 * tener que partirlos como obligaría UDP. Mantiene la misma interfaz que el
 * antiguo VideoMqttClient para que CameraCapture no cambie su lógica.
 */
class VideoTcpClient {
public:
  void begin() { _connect(); }

  /** Llamar cada iteración de la tarea de cámara: reconecta si se cayó. */
  void maintain() {
    if (!_client.connected()) _connect();
  }

  bool isConnected() { return _client.connected(); }

  /** Envía un frame con su cabecera de longitud. Devuelve true si salió completo. */
  bool publishBytes(const char* /*topic*/, const uint8_t* data, size_t len) {
    if (!_client.connected()) return false;
    uint8_t header[4] = {
      (uint8_t)((len >> 24) & 0xFF),
      (uint8_t)((len >> 16) & 0xFF),
      (uint8_t)((len >> 8) & 0xFF),
      (uint8_t)(len & 0xFF)
    };
    if (_client.write(header, 4) != 4) return false;
    return _client.write(data, len) == len;
  }

private:
  WiFiClient    _client;
  unsigned long _lastAttempt = 0;

  void _connect() {
    unsigned long now = millis();
    if (now - _lastAttempt < 3000) return;   // reintento cada 3s, no bloqueante
    _lastAttempt = now;

    Serial.printf("[VideoTCP] Conectando a %s:%d ...\n", EDGE_VIDEO_HOST, EDGE_VIDEO_PORT);
    if (_client.connect(EDGE_VIDEO_HOST, EDGE_VIDEO_PORT)) {
      _client.setNoDelay(true);   // baja latencia: no acumular antes de enviar
      Serial.println("[VideoTCP] Conectado al Edge.");
    } else {
      Serial.println("[VideoTCP] Fallo al conectar, reintentaré.");
    }
  }
};
