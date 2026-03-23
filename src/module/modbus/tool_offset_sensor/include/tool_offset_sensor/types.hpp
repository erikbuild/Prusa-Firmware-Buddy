/// @file
#pragma once

#include <cstdint>
#include <optional>

namespace tool_offset_sensor {

struct Config {
    std::optional<bool> ch0_enabled;
    std::optional<bool> ch1_enabled;

    auto operator<=>(const Config &) const = default;
};

struct Status {
    bool ch0_active = false;
    bool ch1_active = false;
    bool sensor_fault = false;
    uint8_t sensor_errors = 0;
};
} // namespace tool_offset_sensor
