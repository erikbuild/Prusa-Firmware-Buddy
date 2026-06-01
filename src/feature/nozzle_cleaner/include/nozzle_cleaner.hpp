#pragma once

#include <gcode/inject_queue_actions.hpp>
#include <optional>
#include <string_view>
#include <str_utils.hpp>

#include <option/has_indx.h>

namespace nozzle_cleaner {

enum class Sequence : uint16_t {
    clean,
#if HAS_INDX()
    quick_clean,
    deep_clean,
#endif
    purge_clean,
#if HAS_INDX()
    power_panic_purge,
    eject_blob,
    enter_cleaner,
    exit_cleaner,

    // Internal-only sequences below; not invocable via G12.
    _cnt_external,
    enter_cleaner_from_inside = _cnt_external,
#endif
    _cnt,
};

/// Count of sequences exposed via G12. Excludes internal-only sequences.
#if HAS_INDX()
inline constexpr size_t externally_invocable_count = static_cast<size_t>(Sequence::_cnt_external);
#else
inline constexpr size_t externally_invocable_count = static_cast<size_t>(Sequence::_cnt);
#endif

std::optional<Sequence> parse_sequence(std::string_view name);
const GCodeFile &get_sequence(Sequence seq);
void load_sequence(Sequence seq);

/// Load a sequence, wait for it to be ready, and execute it.
/// @return true on success, false if aborted (planner draining)
bool load_and_execute(Sequence seq);

bool is_loader_idle();
bool is_loader_buffering();

/**
 * @brief Executes the loaded nozzle cleaner gcode.
 * The load_sequence() function must be called before this function, and gcode loaded must be in ready state for this to work correctly.
 *
 * @return true if the gcode was executed successfully
 * @return false if still buffering, failed loading or not even loaded.
 */
bool execute();

void reset();

} // namespace nozzle_cleaner
