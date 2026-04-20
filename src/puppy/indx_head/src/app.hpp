//@file
#pragma once

#include <cstdint>

#include <indx_head/leds.hpp>

namespace app {
void run();
int16_t get_nozzle_temp();
void set_led_config(const indx_head::leds::LedConfig cfg);

void set_printfan_pwm(uint8_t pwm);
uint8_t get_printfan_pwm();
uint8_t get_boardfan_pwm();

bool is_printfan_rpm_ok();
bool is_boardfan_rpm_ok();
} // namespace app
