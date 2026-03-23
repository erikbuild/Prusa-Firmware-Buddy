///@file
#include <puppies/tool_offset_sensor.hpp>

#include <modbus/server_address.hpp>

using Lock = std::unique_lock<freertos::Mutex>;
using xbuddy_extension::NodeState;

static constexpr uint8_t unit = std::to_underlying(modbus::ServerAddress::tool_offset_sensor);

namespace buddy::puppies {

NodeState ToolOffsetSensor::get_node_state() const {
    Lock lock(mutex);

    return valid ? static_cast<NodeState>(status.value.node_state) : NodeState::unknown;
}

bool ToolOffsetSensor::has_sensor_fault() const {
    Lock lock(mutex);
    return valid && (status.value.channel_flags & tool_offset_sensor::modbus::channel_flag_sensor_fault) != 0;
}

bool ToolOffsetSensor::has_errors() const {
    Lock lock(mutex);
    if (!valid) {
        return false;
    }
    return (status.value.channel_flags & tool_offset_sensor::modbus::channel_flag_sensor_fault) != 0
        || status.value.sensor_errors != 0;
}

CommunicationStatus ToolOffsetSensor::initial_scan(PuppyModbus &bus) {
    Lock lock(mutex);

    const auto input = refresh_input(bus, 0);
    return input;
}

CommunicationStatus ToolOffsetSensor::refresh_input(PuppyModbus &bus, uint32_t max_age) {
    // Already locked by caller.

    const auto result = bus.read(unit, status, max_age);

    switch (result) {
    case CommunicationStatus::OK:
        valid = true;
        break;
    case CommunicationStatus::ERROR:
        valid = false;
        break;
    default:
        // SKIPPED doesn't change the validity.
        break;
    }

    return result;
}

void ToolOffsetSensor::set_config(bool ch0_enabled, bool ch1_enabled) {
    Lock lock(mutex);
    config.value.ch0_enabled = ch0_enabled;
    config.value.ch1_enabled = ch1_enabled;
    config.dirty = true;
}

CommunicationStatus ToolOffsetSensor::refresh_holding(PuppyModbus &bus) {
    return bus.write(unit, config);
}

CommunicationStatus ToolOffsetSensor::refresh(PuppyModbus &bus) {
    Lock lock(mutex);

    const auto input = refresh_input(bus, 250);
    if (input == CommunicationStatus::ERROR) {
        return CommunicationStatus::ERROR;
    }

    const auto holding = refresh_holding(bus);
    if (holding == CommunicationStatus::ERROR) {
        return CommunicationStatus::ERROR;
    }

    if (input == CommunicationStatus::SKIPPED && holding == CommunicationStatus::SKIPPED) {
        return CommunicationStatus::SKIPPED;
    }
    return CommunicationStatus::OK;
}

ToolOffsetSensor tool_offset_sensor;

} // namespace buddy::puppies
