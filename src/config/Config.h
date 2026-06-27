#pragma once

// Credenciales reales (WiFi + MQTT) viven en Secrets.h, que NO se sube al
// repositorio. Si este archivo no existe, copia Secrets.example.h como
// Secrets.h y completa tus valores antes de compilar.
#include "Secrets.h"

// ── MQTT broker ──────────────────────────────────────────────────
#define MQTT_BROKER_PORT   1883
#define MQTT_CLIENT_ID       "nexbell-esp32s3-door"        // main connection: commands, audio, sensors (Core 1)
#define MQTT_VIDEO_CLIENT_ID "nexbell-esp32s3-door-video"  // dedicated connection: video only (Core 0) — must differ, or the broker disconnects whichever connected first

// ── MQTT topics ──────────────────────────────────────────────────
#define TOPIC_PRESENCE      "nexbell/telemetry/presence"
#define TOPIC_DOOR_ALARM    "nexbell/alarms/door"
#define TOPIC_UNLOCK_CMD    "nexbell/commands/unlock"
#define TOPIC_BELL_BUTTON   "nexbell/alarms/bell" // Tópico del timbre
#define TOPIC_VIBRATION_ALARM "nexbell/alarms/vibration" // Tópico de sensor de vibración
#define TOPIC_AUDIO_PLAYBACK "nexbell/audio/playback"
#define TOPIC_AUDIO_START    "nexbell/audio/start"    // payload "START"/"STOP" — live mic streaming toggle
#define TOPIC_AUDIO_CHUNK    "nexbell/audio/chunk"    // raw PCM bytes streamed live (no Base64/JSON)
#define TOPIC_CAMERA_TRIGGER "nexbell/commands/capture"

// ── HC-SR04 (Ultrasonic / Presence) ──────────────────────────────
#define HC_TRIG_PIN         39
#define HC_ECHO_PIN         40
#define PRESENCE_THRESHOLD_CM  30    // detect if object within 30 cm — tighter range avoids picking up static background objects (desk, wall) as "presence"
#define PRESENCE_POLL_MS      500    // poll every 500 ms
#define PRESENCE_HEARTBEAT_MS 8000   // re-publish "1" every 8s while presence persists (refreshes the Edge's 20s safety timeout)
#define PRESENCE_ON_READINGS  2       // lecturas seguidas para PRENDER presencia (~1s) — reacción rápida
#define PRESENCE_OFF_READINGS 6       // lecturas seguidas para APAGAR presencia (~3s) — evita cortes por baches

// ── SENSORES DE SEGURIDAD FÍSICA ─────────────────────────────────
#define MC38_PIN            41       // GPIO connected to MC38 output (Puerta)
#define VIBRATION_SENSOR_PIN 47      // Sensor de vibración (Tamper/Golpes)

// ── INTERFAZ DE USUARIO (Botones) ────────────────────────────────
#define BUTTON_BELL_PIN      46      // Botón principal (Timbre)
#define BUTTON_EXTRA_PIN     45      // Botón secundario auxiliar

// ── AUDIO: INMP441 (Micrófono I2S - Entrada) ─────────────────────
#define I2S_WS_PIN          21       // Reloj WS compartido
#define I2S_SCK_PIN         42       // Reloj SCK compartido
#define I2S_SD_PIN          38       // Tubería de entrada de voz
#define I2S_PORT            I2S_NUM_0 // Puerto exclusivo de entrada

// ── AUDIO: MAX98357A (Amplificador I2S - Salida) ─────────────────
#define I2S_AMP_LRC_PIN     21       // Apunta al mismo reloj WS
#define I2S_AMP_BCLK_PIN    42       // Apunta al mismo reloj SCK
#define I2S_AMP_DIN_PIN     2        // Tubería de salida de sonido
#define I2S_PORT_AMP        I2S_NUM_1 // Puerto exclusivo de salida

// ── Audio en vivo por UDP (fuera de MQTT) ────────────────────────
// El audio de voz ya NO viaja por el broker MQTT: va por un socket UDP
// directo entre el ESP32 y el Edge service. Esto quita el salto del broker
// y el overhead de PubSubClient (latencia mucho menor) y libera el WiFi/MQTT
// para el video. El control (START/STOP del micrófono) sí sigue por MQTT.
#define EDGE_AUDIO_HOST     MQTT_BROKER_HOST  // el Edge corre en la misma PC que el broker
#define EDGE_AUDIO_PORT     3101              // puerto donde el Edge escucha el mic del ESP32
#define ESP32_AUDIO_PORT    3102              // puerto donde el ESP32 escucha el audio del portero

// ── Audio capture window ─────────────────────────────────────────
#define I2S_SAMPLE_RATE     16000
#define I2S_BUFFER_LEN      512      // samples per DMA block
#define AUDIO_CHUNK_SIZE    1024     // bytes per MQTT chunk
#define AUDIO_RECORD_SECONDS  5
// Máximo de bloques de audio a publicar por cada vuelta del loop principal.
// El loop corre ~cada 50 ms pero a 16 kHz se generan ~800 muestras en ese
// tiempo (más de un bloque). Drenamos varios bloques por ciclo para no
// quedarnos atrás del micrófono y evitar huecos en el audio IoT→PC.
#define AUDIO_MAX_BLOCKS_PER_POLL 6

// ── OV2640 Camera pin mapping ────────────────────────────────────
// Adjust these GPIO numbers to match your actual wiring.
// Common AI-Thinker / bare-module wiring is shown below.
#define CAM_PIN_PWDN    -1   // Power-down: -1 = not used
#define CAM_PIN_RESET   -1   // Reset:      -1 = not used
#define CAM_PIN_XCLK    15
#define CAM_PIN_SIOD    4    // I2C SDA (SCCB)
#define CAM_PIN_SIOC    5    // I2C SCL (SCCB)
#define CAM_PIN_D7      16
#define CAM_PIN_D6      17
#define CAM_PIN_D5      18
#define CAM_PIN_D4      12
#define CAM_PIN_D3      10
#define CAM_PIN_D2      8
#define CAM_PIN_D1      9
#define CAM_PIN_D0      11
#define CAM_PIN_VSYNC   6
#define CAM_PIN_HREF    7
#define CAM_PIN_PCLK    13