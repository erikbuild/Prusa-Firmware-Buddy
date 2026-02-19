#pragma once

#include <cstdint>

namespace hal {

enum class BoardOrientation : uint8_t {
    normal,
    left,
    right
};

/// Enable CAN bit rate switch?
static constexpr const bool enable_bit_rate_switch = false;

void init();
void set_status_led(bool set);

void __attribute__((noreturn)) panic();
void __attribute__((noreturn)) reset();

} // namespace hal
