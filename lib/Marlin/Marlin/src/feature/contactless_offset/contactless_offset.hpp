#pragma once
#include "tool_sensor.hpp"
#include "config.hpp"
#include <expected>

namespace tool_offset {

enum class ToolType {
    STANDARD,
    HARDENED
};

struct ToolOffset {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    ToolType type = ToolType::STANDARD;
};

// Measure the current tool's XYZ offset relative to the sensor reference position.
// Performs homing, Z probing, and two-pass XY scanning internally.
std::expected<ToolOffset, const char *> measure_current_tool_offset(
    const ProbingConfig &config,
    Sensor &sensor);

} // namespace tool_offset
