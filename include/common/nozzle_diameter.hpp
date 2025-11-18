#pragma once

#include <numeric_input_config.hpp>

static constexpr NumericInputConfig nozzle_diameter_spin_config {
    .min_value = 0.1f,
    .max_value = 1.8f,
    .step = 0.05f,
    .max_decimal_places = 2,
    .unit = Unit::millimeter,
};
