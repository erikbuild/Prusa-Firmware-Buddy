/// @file
#include <ac_controller/modbus.hpp>

#include <modbus/traits.hpp>

static_assert(modbus::RegisterFile<ac_controller::modbus::Status>);
static_assert(modbus::RegisterFile<ac_controller::modbus::Config>);
static_assert(modbus::RegisterFile<ac_controller::modbus::LedConfig>);
