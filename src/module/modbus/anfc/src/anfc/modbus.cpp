/// @file
#include <anfc/modbus.hpp>

#include <modbus/traits.hpp>
#include <prusa3d/nfc/command/AcceptEvent_1_0.h>
#include <prusa3d/nfc/command/Request_1_0.h>
#include <prusa3d/nfc/event/Event_1_0.h>

static_assert(modbus::RegisterFile<anfc::modbus::Event>);
static_assert(modbus::RegisterFile<anfc::modbus::Request>);
static_assert(modbus::RegisterFile<anfc::modbus::AcceptEvent>);

constexpr size_t round_up_to_register(size_t bytes) {
    return (bytes + 1) & ~1;
}

static_assert(sizeof(anfc::modbus::Request::data) == round_up_to_register(prusa3d_nfc_command_Request_Request_1_0_EXTENT_BYTES_));
static_assert(sizeof(anfc::modbus::AcceptEvent::data) == round_up_to_register(prusa3d_nfc_command_AcceptEvent_Request_1_0_EXTENT_BYTES_));
static_assert(sizeof(anfc::modbus::Event::data) == round_up_to_register(prusa3d_nfc_event_Event_1_0_EXTENT_BYTES_));

using Device = anfc::Device;
using ServerAddress = modbus::ServerAddress;

ServerAddress anfc::modbus::server_address(Device device) {
    switch (device) {
    case Device::anfc0:
        return ServerAddress::anfc0;
    case Device::anfc1:
        return ServerAddress::anfc1;
    }
    return ServerAddress::invalid;
}
