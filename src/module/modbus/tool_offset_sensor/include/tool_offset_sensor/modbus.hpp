/// @file
#pragma once

#include <cstdint>

/// This file defines MODBUS register files, to be shared between client and server.
/// Resist the temptation to make this type-safe in any way! This is only used for
/// memory layout and should consist of 16-bit values, arrays and structures of such.
/// To ensure proper synchronization, you must always read/write entire register files.

namespace tool_offset_sensor::modbus {

/// MODBUS register file for reporting current status of tool_offset_sensor to xBuddy.
struct Status {
    static constexpr uint16_t address = 0x8000;

    uint16_t node_state; ///< corresponds to xbuddy_extension::NodeState
};

} // namespace tool_offset_sensor::modbus
