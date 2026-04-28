//@file
#pragma once

#include <cstdint>

#include <indx_head/nozzle_presence.hpp>
#include <indx_head/leds.hpp>

namespace app {
void run();

/// In 1/100 °C
int16_t get_nozzle_temp_uncompensated_c100();

/// In 1/100 °C
int16_t get_nozzle_temp_compensated_c100();

/// In 1/100 °C/s
int16_t get_hotend_temp_raw_c100_dt_s();

/// In 1/100 °C
int16_t get_tpis_ambient_temp_c100();

void set_nozzle_present(indx_head::NozzlePresence state);
indx_head::NozzlePresence get_nozzle_present();
void invalidate_nozzle_presence(uint16_t ack_value); ///< Reset debouncer and set nozzle state to unknown, store ack for buddy to read
uint16_t get_nozzle_invalidation_ack();
void set_nozzle_target_temp(uint16_t temp);
void set_led_config(const indx_head::leds::LedConfig cfg);

void set_printfan_pwm(uint8_t pwm);
uint8_t get_printfan_pwm();
uint8_t get_heatbreak_fan_pwm();

bool is_printfan_rpm_ok();
bool is_heatbreak_fan_rpm_ok();

void set_selftest_mode(bool enabled);
} // namespace app
