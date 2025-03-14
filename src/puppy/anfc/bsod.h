#pragma once

#include <hal.hpp>

// Used by stdext::inplace_function
#define bsod(...) hal::panic()
