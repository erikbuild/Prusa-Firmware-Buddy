#pragma once

#include <cstdint>

/**
 * @brief Shared between xBuddy and INDX_HEAD
 */
namespace indx_head::errors {
enum class FaultStatusMask : uint16_t {
    // Priority is set in ascending order (higher number -> higher priority)
    no_fault = 0,
    undefined_error = 1,
    nozzle_min_temp = (1 << 1),
    tpis_invalid_timeout = (1 << 2),
    nozzle_max_temp = (1 << 3),
    hard_fault = (1 << 4),
    stack_overflow = (1 << 5),
    assert_failed = (1 << 6),
    loadcell_crc_mismatch = (1 << 7),
    watchdog_reset = (1 << 8),
    pin_reset = (1 << 9),
    power_reset = (1 << 10),
    board_min_temp = (1 << 11),
    board_max_temp = (1 << 12),
    tpis_ambient_min_temp = (1 << 13),
    tpis_ambient_max_temp = (1 << 14),
};

} // namespace indx_head::errors
