#include "accelerometer.h"

#include <utils/enum_array.hpp>

using namespace accelerometer;
const char *PrusaAccelerometer::error_str() const {
    static constexpr EnumArray<Error, const char *, Error::_cnt> error_strs {
        { Error::none, nullptr },
        { Error::communication, "accelerometer_communication" },
        { Error::no_active_tool, "no active tool" },
        { Error::busy, "busy" },
        { Error::overflow_sensor, "sample overrun on accelerometer sensor" },
        { Error::overflow_buddy, "sample missed on buddy" },
        { Error::overflow_dwarf, "sample missed on dwarf" },
        { Error::overflow_possible, "sample possibly lost in transfer" },
    };
    return error_strs[get_error()];
}
