#pragma once

#include <span>
#include <cstddef>

namespace hal {

/// Enable CAN bit rate switch?
static constexpr const bool enable_bit_rate_switch = false;

void init();
void set_status_led(bool set);

void __attribute__((noreturn)) panic();
void __attribute__((noreturn)) reset();

} // namespace hal
