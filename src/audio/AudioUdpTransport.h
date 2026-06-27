#pragma once
#include <Arduino.h>
#include <WiFiUdp.h>
#include "../config/Config.h"

/**
 * AudioUdpTransport — transporte de audio en vivo por UDP (sin MQTT).
 *
 * Saca el flujo de voz del broker MQTT y lo manda por un socket UDP directo
 * entre el ESP32 y el Edge service:
 *   - El micrófono (mic) se envía a EDGE_AUDIO_HOST:EDGE_AUDIO_PORT.
 *   - El audio del portero llega a este equipo en el puerto ESP32_AUDIO_PORT.
 *
 * UDP es ideal para voz en vivo: si se pierde un paquete, simplemente se
 * ignora (un micro-glitch inaudible) en vez de reenviarse y meter retraso.
 */
class AudioUdpTransport {
public:
  void begin() {
    _udp.begin(ESP32_AUDIO_PORT);   // escuchar audio entrante del portero
    Serial.printf("[AudioUDP] Escuchando audio del portero en UDP %d. Mic -> %s:%d\n",
                  ESP32_AUDIO_PORT, EDGE_AUDIO_HOST, EDGE_AUDIO_PORT);
  }

  /** Envía un bloque de PCM del micrófono al Edge por UDP. */
  bool sendMic(const uint8_t* data, size_t len) {
    if (_udp.beginPacket(EDGE_AUDIO_HOST, EDGE_AUDIO_PORT) != 1) return false;
    _udp.write(data, len);
    return _udp.endPacket() == 1;   // 1 = enviado
  }

  /**
   * Lee un paquete de audio del portero si hay alguno disponible (no bloquea).
   * Devuelve la cantidad de bytes leídos en `buf`, o 0 si no había nada.
   */
  int receivePlayback(uint8_t* buf, size_t maxLen) {
    int packetSize = _udp.parsePacket();
    if (packetSize <= 0) return 0;
    return _udp.read(buf, maxLen);
  }

private:
  WiFiUDP _udp;
};
