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
};
} // namespace tool_offset_sensor
