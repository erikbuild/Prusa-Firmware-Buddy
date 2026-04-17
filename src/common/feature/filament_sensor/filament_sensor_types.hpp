/**
 * @file filament_sensor_types.hpp
 */

#pragma once

#include "stdint.h"
#include <feature/filament_sensor/filament_sensor.hpp>
#include <optional>
#include <array>

enum class LogicalFilamentSensor : uint8_t {
    /// Filament sensor on the current extruder
    extruder,

    /// Side sensor for the current extruder
    /// MK4+MMU: MMU sensor | XL: current side sensor | OTHER: none
    side,

    /// The first runout filament sensor - the one further from the extruder
    /// XL: side sensor | MK4+MMU: MMU sensor | OTHER: extruder sensor
    primary_runout,

    /// Filament that is closest to the nozzle
    /// INDX: side | OTHER: extruder
    closest_to_nozzle,

    /// First filament sensor that can be triggered without extruder assistance
    /// With MMU rework, it is NOT the extruder sensor, because that one is coupled to the gear
    /// MK+MMU, iX: side | INDX: side | OTHER: extruder
    closest_to_nozzle_independent,
};

static constexpr size_t logical_filament_sensor_count = 5;

struct LogicalFilamentSensors {
    std::array<std::atomic<IFSensor *>, logical_filament_sensor_count> array = { nullptr };

    inline auto &operator[](LogicalFilamentSensor fs) {
        return array.at(std::to_underlying(fs));
    }
    inline IFSensor *operator[](LogicalFilamentSensor fs) const {
        return array.at(std::to_underlying(fs));
    }
};

// We need those. States obtained from from sensors directly might not by synchronized
struct LogicalFilamentSensorStates {
    using State = std::atomic<FilamentSensorState>;
    static constexpr const FilamentSensorState init_val = FilamentSensorState::NotInitialized;
    std::array<State, logical_filament_sensor_count> array = { init_val };

    inline auto &operator[](LogicalFilamentSensor fs) {
        return array.at(std::to_underlying(fs));
    }
    inline const auto &operator[](LogicalFilamentSensor fs) const {
        return array.at(std::to_underlying(fs));
    }
};
