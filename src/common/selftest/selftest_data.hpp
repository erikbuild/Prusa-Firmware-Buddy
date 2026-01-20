#pragma once

#include <variant>
#include <cstdint>
#include <type_traits>

#include "selftest_types.hpp"

namespace selftest {

struct FirstLayerCalibrationData {
    uint8_t previous_sheet;
};

using TestData = std::variant<std::monostate, ToolMask, FirstLayerCalibrationData>;

} // namespace selftest
