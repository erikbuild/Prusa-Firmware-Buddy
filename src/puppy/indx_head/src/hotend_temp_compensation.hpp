/// @file
#pragma once

#include <fpm/fixed.hpp>

// The thermometer on the head is basically not close enough to the melt zone
// and there is a significant difference between the measured temperature and what temp the filament is actually on
// To compensate for this, we calculate a rudimentary filament model on the motherboard
// and send a compensation parameter over the modbus to the head.
// Here, we just apply some expoential fadeout to smoothen things out.
// BFW-8630
namespace hotend_temp_compensation {

/// Steps the compensation fadeout
/// Called from the app task
void step();

/// Sets the compensation target, in 1/100 °C
/// Calculated on the mobo, sent over modbus
void set_target_compensation_c100(int16_t set);

/// In 1/100 °C
/// Thread-safe
int16_t get_current_compensation_c100();

} // namespace hotend_temp_compensation
