#pragma once

#include <cstdint>

/**
 * @brief Shared between xBuddy and INDX_HEAD
 */
namespace indx_head {

/// Tristate nozzle presence reported over modbus.
enum class NozzlePresence : uint16_t {
    unknown = 0, ///< Nothing detected yet
    present = 1, ///< Nozzle presence detected
    absent = 2, ///< Nozzle absence detected
};

} // namespace indx_head
