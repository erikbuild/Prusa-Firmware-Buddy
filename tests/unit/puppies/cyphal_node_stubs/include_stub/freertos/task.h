#pragma once

using TaskHandle_t = void *;

static constexpr int eSetBits = 0;

inline TaskHandle_t xTaskGetCurrentTaskHandle() { return nullptr; }

inline void xTaskNotifyIndexed(...) {}
inline bool xTaskNotifyIndexedFromISR(...) { return true; }
inline bool xTaskNotifyWaitIndexed(...) { return true; }

inline void vTaskDelay(...) {}
