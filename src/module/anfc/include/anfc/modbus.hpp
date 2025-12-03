/// @file
#pragma once

#include <anfc/types.hpp>
#include <array>
#include <cstdint>
#include <modbus/server_address.hpp>

/// This file defines MODBUS register files, to be shared between client and server.
/// Resist the temptation to make this type-safe in any way! This is only used for
/// memory layout and should consist of 16-bit values, arrays and structures of such.
/// To ensure proper synchronization, you must always read/write entire register files.

namespace anfc::modbus {

/// MODBUS register file for reporting last Event to motherboard.
struct Event {
    static constexpr uint16_t address = 0x8000;

    uint16_t size; ///< how many valid bytes are in data
    std::array<uint16_t, 62> data; ///< actual bytes of the payload (little endian)
};

/// MODBUS register file for transmitting AcceptEvent command from motherboard.
struct AcceptEvent {
    static constexpr uint16_t address = 0x9000;

    uint16_t size; ///< how many valid bytes are in data
    std::array<uint16_t, 1> data; ///< actual bytes of the payload (little endian)
};

/// MODBUS register file for transmitting Request command from motherboard.
struct Request {
    static constexpr uint16_t address = 0x9100;

    uint16_t size; ///< how many valid bytes are in data
    std::array<uint16_t, 62> data; ///< actual bytes of the payload (little endian)
};

/// Interface used to decouple dependency on actual MODBUS implementation.
class Client {
protected:
    ~Client() = default;

public:
    [[nodiscard]] virtual bool read(Device, Event &) = 0;
    [[nodiscard]] virtual bool write(Device, const Request &) = 0;
    [[nodiscard]] virtual bool write(Device, const AcceptEvent &) = 0;
};

::modbus::ServerAddress server_address(Device);

} // namespace anfc::modbus
