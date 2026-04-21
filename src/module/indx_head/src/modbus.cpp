/// @file
#include <indx_head/modbus.hpp>

#include <modbus/traits.hpp>

static_assert(modbus::RegisterFile<indx_head::modbus::Status>);
static_assert(modbus::RegisterFile<indx_head::modbus::Config>);
