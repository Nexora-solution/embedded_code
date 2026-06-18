# NexBell ESP32S3 Firmware

## Hardware Wiring

| Component | Signal | ESP32S3 GPIO |
|-----------|--------|--------------|
| HC-SR04   | TRIG   | GPIO 10      |
| HC-SR04   | ECHO   | GPIO 11      |
| MC38      | OUT    | GPIO 4       |
| INMP441   | WS     | GPIO 5       |
| INMP441   | SCK    | GPIO 6       |
| INMP441   | SD     | GPIO 7       |

## Quick Start

1. Install [PlatformIO IDE](https://platformio.org/) (VS Code extension)
2. Open this folder in PlatformIO
3. Edit `src/config/Config.h` — set `WIFI_SSID`, `WIFI_PASSWORD`, and `MQTT_BROKER_HOST`
4. Build & upload: `pio run --target upload`
5. Monitor serial: `pio device monitor`

## MQTT Topics

| Topic                        | Direction          | Payload         |
|------------------------------|--------------------|-----------------|
| `nexbell/telemetry/presence` | ESP32S3 → Broker  | `"1"` / `"0"`  |
| `nexbell/alarms/door`        | ESP32S3 → Broker  | `"OPEN"` / `"CLOSED"` |
| `nexbell/commands/unlock`    | Broker → ESP32S3  | `"UNLOCK"`      |
| `nexbell/audio/start`        | Broker → ESP32S3  | visitRequestId  |
| `nexbell/audio/chunk`        | ESP32S3 → Broker  | JSON chunk      |
| `nexbell/audio/done`         | ESP32S3 → Broker  | `{"id":<n>}`   |

## Dependencies
- PubSubClient 2.8+
- ArduinoJson 7.x
