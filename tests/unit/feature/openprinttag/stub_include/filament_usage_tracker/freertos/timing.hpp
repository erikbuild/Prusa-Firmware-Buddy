#pragma once

#include <cstdint>

namespace freertos {

inline uint32_t millis_val = 0;

inline uint32_t millis() {
    return millis_val;
}

inline void delay(uint32_t) {}

} // namespace freertos
