///@file
#pragma once

#include "PuppyModbus.hpp"
#include <tool_offset_sensor/modbus.hpp>
#include <freertos/mutex.hpp>
#include <option/has_tool_offset_sensor.h>
#include <xbuddy_extension/shared_enums.hpp>

static_assert(HAS_TOOL_OFFSET_SENSOR());

namespace buddy::puppies {

/// Represents virtual Tool Offset Sensor modbus device
/// This handles synchronization between tasks and caching the data.
class ToolOffsetSensor final {
public:
    xbuddy_extension::NodeState get_node_state() const;

    // These are called from the puppy task.
    CommunicationStatus refresh(PuppyModbus &);
    CommunicationStatus initial_scan(PuppyModbus &);

private:
    // The registers cached here are accessed from different tasks.
    mutable freertos::Mutex mutex;

    bool valid = false;
    bool all_valid() const;

    using Status = tool_offset_sensor::modbus::Status;
    ModbusInputRegisterBlock<Status::address, Status> status;

    CommunicationStatus refresh_input(PuppyModbus &, uint32_t max_age);
};

extern ToolOffsetSensor tool_offset_sensor;

} // namespace buddy::puppies
