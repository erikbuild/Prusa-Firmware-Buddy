/// @file
#pragma once

#include <cstdint>

namespace xbuddy_extension {

enum FilamentSensorState : uint8_t {
    uninitialized,
    no_filament,
    has_filament,
    disconnected,
    _last = disconnected,
};
static constexpr uint8_t bits_per_fs_state = 2;
static_assert(FilamentSensorState::_last < (1 << bits_per_fs_state));

enum class Fan : uint8_t {
    cooling_fan_1 = 0, /// Cooling fans have shared PWM control, separate RPM readouts
    cooling_fan_2 = 1, /// Cooling fans have shared PWM control, separate RPM readouts
    filtration_fan = 2, /// Filtration fan, optional
};

static constexpr uint8_t fan_count = 3;
static constexpr uint8_t ext_filament_sensor_count = 8;

/// High-level cyphal node state for reporting.
enum class NodeState : uint8_t {
    unknown,
    verify,
    flash,
    ready,
};

/// File id used in modbus structures.
enum class FileId : uint8_t {
    none = 0,
    firmware_ac_controller,
    firmware_anfc,
    firmware_tool_offset_sensor
};

} // namespace xbuddy_extension
