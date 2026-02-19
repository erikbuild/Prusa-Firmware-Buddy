#pragma once

#include <cstdint>

#include <hal.hpp>

#include <precise_timer.hpp>

extern precise_timing::Timer precise_timer;

#define TIM_BASE_CLK_MHZ precise_timing::counter_period;

inline int32_t ticks_diff(uint32_t ticks_a, uint32_t ticks_b) {
    return ((int32_t)(ticks_a - ticks_b));
}

inline uint64_t ticks_us() {
    return precise_timer.get_timestamp_us();
}

inline uint32_t ticks_ms() {
    return precise_timer.get_timestamp_ms();
}

inline uint64_t get_timestamp_us() {
    return precise_timer.get_timestamp_us();
}
