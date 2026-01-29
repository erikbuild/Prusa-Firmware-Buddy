/// @file
#pragma once

#include <cstdint>

#include <inc/MarlinConfig.h>

struct HotendRegulatorArgs {
    /// Hotend index
    uint8_t hotend_index;

    /// Speed of the print cooling fan
    /// Used in steady_state_hotend
    uint8_t fan_speed;

    /// Current temperature of the hotend, in °C
    float current_temp;

    /// Target temperature of the hotend, in °C
    int16_t target_temp;

#if ENABLED(PID_EXTRUSION_SCALING)
    /// Delta of the E stepper, in some weird volumetric units
    float e_volume_delta;
#endif
};

struct HotendRegulatorResult {
    float pid_output;
    float feed_forward;
};
