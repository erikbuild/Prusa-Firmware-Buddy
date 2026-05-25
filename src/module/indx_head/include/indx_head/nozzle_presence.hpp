#pragma once

#include <cstdint>

/**
 * @brief Shared between xBuddy and INDX_HEAD
 */
namespace indx_head {

/// Nozzle presence reported over modbus.
/// The INDX_HEAD heater classifies any decay between the present/absent thresholds
/// (e.g. nozzle stuck halfway) as `unknown`; such samples are skipped by the head's
/// debouncer instead of being exposed as a separate state, so xBuddy sees only the
/// last clean present/absent (or nothing, after invalidation) and the existing
/// timeout-based recovery handles the failure case.
enum class NozzlePresence : uint16_t {
    unknown = 0, ///< Nothing detected yet (boot, after invalidation, invalid analysis, or decay between thresholds)
    present = 1, ///< Nozzle presence detected (decay above present threshold)
    absent = 2, ///< Nozzle absence detected (decay below absent threshold)
};

} // namespace indx_head
