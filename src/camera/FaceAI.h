#pragma once
#include <Arduino.h>
#include <stdarg.h>
#include <esp_camera.h>
#include <esp_heap_caps.h>
#include <img_converters.h>   // fmt2rgb888() — parte de esp32-camera
#include "../config/Config.h"

// ── Cabeceras ESP-DL (vienen con el core de Arduino 2.0.x) ──────────
#if FACE_DETECT_ENABLED
  #include "human_face_detect_msr01.hpp"
  #include "human_face_detect_mnp01.hpp"
#endif
#if FACE_RECOGNITION_ENABLED
  #include "face_recognition_tool.hpp"
  #include "face_recognition_112_v1_s8.hpp"
#endif

/**
 * FaceAI — detección + reconocimiento facial on-device (ESP-DL).
 *
 * (Se llama FaceAI y no FaceRecognizer porque ESP-DL ya define una clase
 *  `FaceRecognizer` internamente — habría choque de nombres.)
 *
 * Pensado para RESIDENTES: se registra su cara con la cámara del IoT y, cuando
 * vuelve, se le reconoce. NO abre nada — solo publica un evento para que el
 * backend avise al portero.
 *
 * process() corre en la tarea de cámara (Core 0). Como el MQTT vive en el Core
 * 1 y PubSubClient no es thread-safe, los eventos se encolan (FreeRTOS queue) y
 * el loop principal (Core 1) los saca con takeEvent() y los publica.
 */
class FaceAI {
public:
  void begin() {
    _eventQueue = xQueueCreate(8, sizeof(FaceEvent));
    Serial.println("[Face] Reconocimiento facial listo (ESP-DL).");
  }

  /** Corre la IA sobre un frame JPEG. Llamado desde la tarea de cámara (Core 0). */
  void process(camera_fb_t* fb) {
#if FACE_DETECT_ENABLED
    // Decodificar JPEG -> RGB888 en PSRAM (QVGA = 320x240x3 ≈ 225 KB).
    const size_t rgb_len = fb->width * fb->height * 3;
    uint8_t* rgb_buf = (uint8_t*)heap_caps_malloc(rgb_len, MALLOC_CAP_SPIRAM);
    if (!rgb_buf) return;
    if (!fmt2rgb888(fb->buf, fb->len, PIXFORMAT_JPEG, rgb_buf)) {
      heap_caps_free(rgb_buf);
      return;
    }

    // Etapa 1: detección gruesa (MSR01).
    std::list<dl::detect::result_t> &candidates =
        _s1.infer((uint8_t*)rgb_buf, {(int)fb->height, (int)fb->width, 3});
    if (candidates.empty()) { heap_caps_free(rgb_buf); return; }

    // Etapa 2: refinamiento + keypoints (MNP01).
    std::list<dl::detect::result_t> &results =
        _s2.infer((uint8_t*)rgb_buf, {(int)fb->height, (int)fb->width, 3}, candidates);

    if (results.size() > 0) {
      _push("{\"event\":\"face_detected\",\"count\":%d}", (int)results.size());

#if FACE_RECOGNITION_ENABLED
      for (auto &face : results) {
        if (_enrolling) {
          // ── Modo registro: guardar esta cara ──
          int enrolled = _recognizer.get_enrolled_id_num();
          if (enrolled < FR_MAX_ENROLLED) {
            Tensor<uint8_t> tensor;
            tensor.set_element(rgb_buf).set_shape({(int)fb->height, (int)fb->width, 3}).set_auto_free(false);
            // false = guardar la cara en MEMORIA, no en flash (no tenemos partición
            // de flash para el modelo, y el flash está casi lleno). Las caras viven
            // mientras el ESP32 esté encendido.
            int id = _recognizer.enroll_id(tensor, face.keypoint, "", false);
            if (id >= 0) _push("{\"event\":\"face_enrolled\",\"id\":%d,\"total\":%d}", id, enrolled + 1);
            else        _push("{\"event\":\"face_enroll_failed\"}");
          } else {
            _push("{\"event\":\"face_db_full\",\"max\":%d}", FR_MAX_ENROLLED);
          }
          _enrolling = false;  // un solo registro por comando
          break;
        } else if (_recognizer.get_enrolled_id_num() > 0) {
          // ── Modo reconocimiento ──
          Tensor<uint8_t> tensor;
          tensor.set_element(rgb_buf).set_shape({(int)fb->height, (int)fb->width, 3}).set_auto_free(false);
          face_info_t recognized = _recognizer.recognize(tensor, face.keypoint);
          if (recognized.id >= 0 && recognized.similarity >= FR_CONFIDENCE_THR) {
            _push("{\"event\":\"face_recognized\",\"id\":%d,\"similarity\":%.4f}",
                  recognized.id, recognized.similarity);
          } else {
            _push("{\"event\":\"face_unknown\"}");
          }
        }
      }
#endif  // FACE_RECOGNITION_ENABLED
    }
    heap_caps_free(rgb_buf);  // liberar SIEMPRE (evita agotar la PSRAM)
#endif  // FACE_DETECT_ENABLED
  }

  /** Marca para registrar la siguiente cara detectada (lo dispara el portero). */
  void enrollNext() { _enrolling = true; }

  /** Borra todas las caras registradas. */
  void deleteAll() {
#if FACE_RECOGNITION_ENABLED
    _recognizer.clear_id(false);   // false = solo memoria (sin tocar flash)
    _push("{\"event\":\"faces_deleted\"}");
#endif
  }

  /** Saca un evento de la cola (Core 1) para publicarlo por MQTT. */
  bool takeEvent(char* buf, size_t len) {
    if (!_eventQueue || len == 0) return false;
    FaceEvent ev;
    if (xQueueReceive(_eventQueue, &ev, 0) == pdTRUE) {
      strncpy(buf, ev.json, len - 1);
      buf[len - 1] = '\0';
      return true;
    }
    return false;
  }

private:
  struct FaceEvent { char json[160]; };
  QueueHandle_t _eventQueue = nullptr;
  volatile bool _enrolling = false;

  void _push(const char* fmt, ...) {
    FaceEvent ev;
    va_list args;
    va_start(args, fmt);
    vsnprintf(ev.json, sizeof(ev.json), fmt, args);
    va_end(args);
    Serial.printf("[Face] %s\n", ev.json);
    if (_eventQueue) xQueueSend(_eventQueue, &ev, 0);
  }

#if FACE_DETECT_ENABLED
  HumanFaceDetectMSR01 _s1{FD_S1_SCORE_THR, FD_S1_NMS_THR, FD_S1_TOP_K, FD_S1_RESIZE};
  HumanFaceDetectMNP01 _s2{FD_S2_SCORE_THR, FD_S2_NMS_THR, FD_S2_TOP_K};
#endif
#if FACE_RECOGNITION_ENABLED
  FaceRecognition112V1S8 _recognizer;
#endif
};
