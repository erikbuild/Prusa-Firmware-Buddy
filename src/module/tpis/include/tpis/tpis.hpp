#pragma once

#include <fpm/fixed.hpp>
#include <cstdint>
#include <optional>
#include <span>

namespace tpis {

using fixed = fpm::fixed<int32_t, int64_t, 7>;

constexpr float emissivity = 0.46f;

struct SensorData {
    uint32_t tp_object = 0;
    uint16_t tp_ambient = 0;
};

struct CalibrationParameters {
    uint16_t ptat25 = 0;
    fixed m { 0 };
    uint32_t u0 = 0;
    uint32_t uout1 = 0;
    uint8_t t_obj1 = 0;
    // Could be uint32_t, but to calcualte it we need floats and we would instantly convert it to float anyway
    float k_inv = 0;
};

struct TemperatureReading {
    float object_temperature_celsius;
    float ambient_temperature_celsius;
};

SensorData decode_sensor_data(std::span<const std::byte, 4> raw_data);
std::optional<CalibrationParameters> decode_calibration_parameters(std::span<const std::byte, 32> raw_data);
TemperatureReading calculate_temps(SensorData measurement, const CalibrationParameters &calibration);

} // namespace tpis
