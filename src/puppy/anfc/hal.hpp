#pragma once

#include <cstdint>

namespace hal {
void init();
void status_led_on();
void status_led_off();
void delay(uint32_t ms);
void __attribute__((noreturn)) panic();
} // namespace hal
