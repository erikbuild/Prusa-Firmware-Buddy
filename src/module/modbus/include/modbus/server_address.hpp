#pragma once

#include <cstdint>

namespace modbus {

enum class ServerAddress : uint8_t {
    // Note: sorted alphabetically by server name, not by address

    ac_controller = 0x1a + 8,
    anfc0 = 221,
    anfc1 = 222,
    mmu = 220,
    xbuddy_extension = 0x1a + 7,
};

}
