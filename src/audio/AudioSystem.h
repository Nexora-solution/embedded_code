#pragma once
#include <Arduino.h>
#include <driver/i2s.h>
#include "../config/Config.h"

/**
 * Sistema Unificado de Audio (Silencio Cero)
 * 
 * Inicializa el puerto I2S (I2S_NUM_0) en modo Full-Duplex (Master TX/RX).
 * Al centralizar la inicialización, evitamos que el micrófono y el altavoz
 * compitan por los mismos pines de reloj.
 * Utiliza `tx_desc_auto_clear = true` para emitir ceros por el altavoz 
 * automáticamente cuando no hay audio, evitando la molesta estática.
 */
class AudioSystem {
public:
  static void inicializarAudioSilencioso() {
    i2s_config_t cfg = {
      .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_RX),
      .sample_rate          = I2S_SAMPLE_RATE, // 16000
      .bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT,
      .channel_format       = I2S_CHANNEL_FMT_ONLY_LEFT,
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count        = 8,
      .dma_buf_len          = 64,
      .use_apll             = false,
      .tx_desc_auto_clear   = true, // 🛡️ EL ESCUDO ANTI-ESTÁTICA
      .fixed_mclk           = 0,
    };

    // Usamos los pines compartidos del Config.h
    // Y definimos entrada (Mic) y salida (Amp) en el mismo puerto
    i2s_pin_config_t pins = {
      .mck_io_num   = I2S_PIN_NO_CHANGE,
      .bck_io_num   = I2S_SCK_PIN,       // Pin compartido (42)
      .ws_io_num    = I2S_WS_PIN,        // Pin compartido (21)
      .data_out_num = I2S_AMP_DIN_PIN,   // Salida hacia el MAX98357A (2)
      .data_in_num  = I2S_SD_PIN,        // Entrada desde el INMP441 (38)
    };

    // Instalamos el driver en el puerto base (I2S_PORT_0)
    i2s_driver_install(I2S_PORT, &cfg, 0, NULL);
    i2s_set_pin(I2S_PORT, &pins);
    
    // Limpiamos los buffers de entrada
    i2s_zero_dma_buffer(I2S_PORT);

    // Emitimos un pequeño "Bip" seguido de ceros forzados para estabilizar la membrana
    int16_t silencio = 0;
    size_t bytes_escritos;
    
    // Inyectamos 100ms de ceros
    for (int i = 0; i < 1600; i++) {
       i2s_write(I2S_PORT, &silencio, sizeof(silencio), &bytes_escritos, portMAX_DELAY);
    }
    
    Serial.println("[AudioSystem] Bus I2S Full-Duplex inicializado. Altavoz en modo Silencio Cero.");
  }
};
