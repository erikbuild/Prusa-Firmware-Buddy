/// @file
#pragma once

#include <cstdint>

#include <inc/MarlinConfig.h>
#include <module/temperature/temp_defines.hpp>

static_assert(ENABLED(PIDTEMP));
static_assert(HAS_PID_HEATING);

struct HotendPIDConfig {
    float Kp = DEFAULT_Kp;

    /// The value is PRE-SCALED by scalePID_i
    float Ki = scalePID_i(DEFAULT_Ki);

    /// The value is PRE-SCALED by scalePID_d
    float Kd = scalePID_d(DEFAULT_Kd);

#if ENABLED(PID_EXTRUSION_SCALING)
    float Kc = DEFAULT_Kc;
#endif
};

struct HotendRegulatorArgs {
    const HotendPIDConfig &pid;

    /// Hotend index
    uint8_t hotend_index;

    /// Speed of the print cooling fan
    /// Used in steady_state_hotend
    uint8_t fan_speed;

    /// Current temperature of the hotend, in °C
    float current_temp;

    /// Target temperature of the hotend, in °C
    int16_t target_temp;

    /// If set, forcefully resets the PID
    bool reset_pid = false;

#if ENABLED(PID_EXTRUSION_SCALING)
    /// Delta of the E stepper, in some weird volumetric units
    float e_volume_delta;
#endif
};

struct HotendRegulatorResult {
    float pid_output;
    float feed_forward;
};
