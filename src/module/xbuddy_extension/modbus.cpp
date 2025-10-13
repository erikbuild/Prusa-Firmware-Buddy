/// @file
#include <xbuddy_extension/modbus.hpp>

#include <type_traits>

static_assert(std::is_standard_layout_v<xbuddy_extension::modbus::Status>);
static_assert(std::is_standard_layout_v<xbuddy_extension::modbus::Config>);
