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

CommunicationStatus ToolOffsetSensor::refresh(PuppyModbus &bus) {
    Lock lock(mutex);

    const auto input = refresh_input(bus, 250);

    if (input == CommunicationStatus::ERROR) {
        return CommunicationStatus::ERROR;
    } else if (input == CommunicationStatus::SKIPPED) {
        return CommunicationStatus::SKIPPED;
    } else {
        return CommunicationStatus::OK;
    }
}

ToolOffsetSensor tool_offset_sensor;

} // namespace buddy::puppies
