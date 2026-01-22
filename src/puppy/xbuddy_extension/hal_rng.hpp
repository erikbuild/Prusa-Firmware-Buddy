/// @file
#pragma once

#include <cstdint>

namespace hal::rng {

/// Initialize truly random number generator peripheral.
void init();

/// Obtain a truly random number.
uint32_t get();

} // namespace hal::rng
