#pragma once

// ── Wi-Fi credentials ────────────────────────────────────────────
#define WIFI_SSID      "Sdarxx"
#define WIFI_PASSWORD  "sack136_noob"

// ── MQTT broker ──────────────────────────────────────────────────
#define MQTT_BROKER_HOST   "10.157.107.145"   // IP of Mosquitto broker
#define MQTT_BROKER_PORT   1883
#define MQTT_CLIENT_ID     "nexbell-esp32s3-door"

// ── MQTT topics ──────────────────────────────────────────────────
#define TOPIC_PRESENCE      "nexbell/telemetry/presence"
#define TOPIC_DOOR_ALARM    "nexbell/alarms/door"
#define TOPIC_UNLOCK_CMD    "nexbell/commands/unlock"

// ── HC-SR04 (Ultrasonic / Presence) ──────────────────────────────
#define HC_TRIG_PIN         39
#define HC_ECHO_PIN         40
#define PRESENCE_THRESHOLD_CM  80    // detect if object within 80 cm
#define PRESENCE_POLL_MS      500    // poll every 500 ms

// ── MC38 (Magnetic door sensor) ──────────────────────────────────
#define MC38_PIN            41        // GPIO connected to MC38 output

// ── INMP441 (I2S Microphone) ─────────────────────────────────────
#define I2S_WS_PIN          21
#define I2S_SCK_PIN         42
#define I2S_SD_PIN          38
#define I2S_PORT            I2S_NUM_0
#define I2S_SAMPLE_RATE     16000
#define I2S_BUFFER_LEN      512      // samples per DMA block
#define AUDIO_CHUNK_SIZE    1024     // bytes per MQTT chunk

// ── Audio capture window ─────────────────────────────────────────
#define AUDIO_RECORD_SECONDS  5

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
