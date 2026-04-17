#pragma once

#include <optional>
#include <string_view>
#include <str_utils.hpp>

#include <option/has_indx.h>

namespace nozzle_cleaner {

enum class Sequence : uint16_t {
    clean = 0,
#if HAS_INDX()
    quick_clean = 1,
    deep_clean = 2,
#endif
    // Reserved for more cleaning sequences
    purge_clean = 20,
// Reserved for more purge sequences
#if HAS_INDX()
    eject_blob = 30,
    // Reserved for other sequences

    enter_cleaner = 90,
    exit_cleaner = 91,
// Feel free to add anything above 100, totaly free range
#endif
};

struct SequenceGCode {
    ConstexprString filename;
    ConstexprString sequence;
};

std::optional<Sequence> parse_sequence(std::string_view name);
bool is_valid_sequence(Sequence seq);
const SequenceGCode &get_sequence(Sequence seq);
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
