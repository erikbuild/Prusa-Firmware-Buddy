/// @file
#pragma once

#include <cstdint>
#include <optional>

namespace tool_offset_sensor {

struct Config {
    std::optional<bool> enable_streaming;

    auto operator<=>(const Config &) const = default;
};

struct Status {
};
} // namespace tool_offset_sensor
