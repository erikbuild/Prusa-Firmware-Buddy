/**
 * @file selftest_invocation.hpp
 * @brief Shared lifecycle state for a single selftest invocation.
 *
 * A "selftest invocation" covers one run of a selftest regardless of how it is
 * implemented (through the ISelftest state machine, or as a gcode-based
 * wizard). This module owns the abort flag that needs to bridge those two
 * worlds — set by whoever detects the abort (CSelftest::Abort() or a gcode
 * wizard's abort path), read by the snake to decide whether to stop queuing
 * remaining tests.
 */
#pragma once

namespace selftest_invocation {

/// Reset state for a new invocation. Call before dispatching a selftest.
void begin();

/// Mark the current invocation as aborted. Callable from both gcode wizards
/// and CSelftest::Abort().
void mark_aborted();

/// True if the current invocation was marked aborted.
bool is_aborted();

} // namespace selftest_invocation
