#pragma once

// ── Wi-Fi credentials ────────────────────────────────────────────
#define WIFI_SSID      "YOUR_WIFI_SSID"
#define WIFI_PASSWORD  "YOUR_WIFI_PASSWORD"

// ── MQTT broker ──────────────────────────────────────────────────
#define MQTT_BROKER_HOST   "192.168.1.100"   // IP of Mosquitto broker
#define MQTT_BROKER_PORT   1883
#define MQTT_CLIENT_ID     "nexbell-esp32s3-door"

// ── MQTT topics ──────────────────────────────────────────────────
#define TOPIC_PRESENCE      "nexbell/telemetry/presence"
#define TOPIC_DOOR_ALARM    "nexbell/alarms/door"
#define TOPIC_UNLOCK_CMD    "nexbell/commands/unlock"

// ── HC-SR04 (Ultrasonic / Presence) ──────────────────────────────
#define HC_TRIG_PIN         10
#define HC_ECHO_PIN         11
#define PRESENCE_THRESHOLD_CM  80    // detect if object within 80 cm
#define PRESENCE_POLL_MS      500    // poll every 500 ms

// ── MC38 (Magnetic door sensor) ──────────────────────────────────
#define MC38_PIN            4        // GPIO connected to MC38 output

// ── INMP441 (I2S Microphone) ─────────────────────────────────────
#define I2S_WS_PIN          5
#define I2S_SCK_PIN         6
#define I2S_SD_PIN          7
#define I2S_PORT            I2S_NUM_0
#define I2S_SAMPLE_RATE     16000
#define I2S_BUFFER_LEN      512      // samples per DMA block
#define AUDIO_CHUNK_SIZE    1024     // bytes per MQTT chunk

// ── Audio capture window ─────────────────────────────────────────
#define AUDIO_RECORD_SECONDS  5
