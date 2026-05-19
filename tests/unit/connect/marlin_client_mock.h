#pragma once

#include <string>

namespace marlin_client {

extern std::string last_gcode;

void gcode_printf(const char *format, ...);

} // namespace marlin_client
