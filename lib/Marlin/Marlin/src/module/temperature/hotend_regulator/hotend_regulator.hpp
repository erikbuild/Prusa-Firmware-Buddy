/// @file
#pragma once

#include <cstdint>

struct HotendRegulatorArgs {
    /// Hotend index
    uint8_t hotend_index;

    /// Speed of the print cooling fan
    /// Used in steady_state_hotend
    uint8_t fan_speed;
};

struct HotendRegulatorResult {
    float pid_output;
    float feed_forward;
};
