#include <accelerometer/common_structs.hpp>

using namespace accelerometer;

Acceleration::Acceleration(float val1, float val2, float val3)
    : val { val1, val2, val3 } {}
Acceleration::Acceleration(const RawAcceleration &raw)
    : val { raw_to_accel(raw.val[0]), raw_to_accel(raw.val[1]), raw_to_accel(raw.val[2]) } {}

Acceleration RawAcceleration::to_acceleration() const {
    return Acceleration { raw_to_accel(val[0]), raw_to_accel(val[1]), raw_to_accel(val[2]) };
}
