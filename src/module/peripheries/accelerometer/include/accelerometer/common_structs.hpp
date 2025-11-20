#pragma once

#include <cstdint>

namespace accelerometer {
struct Acceleration;
struct RawAcceleration;

struct Acceleration {
    Acceleration() = default;
    Acceleration(float val1, float val2, float val3);
    Acceleration(const RawAcceleration &raw);
    float val[3];
};

struct RawAcceleration {
    Acceleration to_acceleration() const;
    int16_t val[3];
};

constexpr float raw_to_accel(const int16_t &raw) {
    static constexpr float standard_gravity = 9.80665f;
    static constexpr int16_t max_value = 0b0111'1111'1111'1111;
    static constexpr float factor2g = 2.f * standard_gravity / max_value;
    return raw * factor2g;
}

enum class Error {
    none,
    communication,
    no_active_tool,
    busy,
    overflow_sensor, ///< Data not consistent, sample overrun on accelerometer sensor
    overflow_buddy, ///< Data not consistent, sample missed on buddy
    overflow_dwarf, ///< Data not consistent, sample missed on dwarf
    overflow_possible, ///< Data not consistent, sample possibly lost in transfer

    _cnt
};
} // namespace accelerometer
