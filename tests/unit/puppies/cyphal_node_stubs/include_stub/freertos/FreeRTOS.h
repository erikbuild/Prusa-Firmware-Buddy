#pragma once

#include <cstdint>

using BaseType_t = int32_t;
using UBaseType_t = uint32_t;
using TickType_t = int64_t;

static constexpr TickType_t portMAX_DELAY = 1;
static constexpr int portTICK_PERIOD_MS = 1;
static constexpr int TIM_BASE_CLK_MHZ = 1;

static constexpr bool pdPASS = true;
static constexpr bool pdTRUE = true;
static constexpr bool pdFALSE = false;

inline int pdMS_TO_TICKS(int val) {
    return val;
}

inline int __get_IPSR() {
    return 0;
}

inline void portYIELD_FROM_ISR(...) {}

inline bool xPortIsInsideInterrupt() { return false; }
