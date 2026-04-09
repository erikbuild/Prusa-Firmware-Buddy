#pragma once
#include <cstdint>

namespace dwarf::accelerometer {
struct AccelerometerRecord {
    union {
        struct {
            int16_t x, y, z;
        };
        int16_t raw[3];
    };
    bool buffer_overflow = false;
    bool sample_overrun = false;
};
} // namespace dwarf::accelerometer
