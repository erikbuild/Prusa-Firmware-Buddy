#include "PrusaGcodeSuite.hpp"

#include <config_store/store_instance.hpp>
#include <gcode/gcode_parser.hpp>
#include <module/motion.h>
#include <module/prusa/toolchanger_utils.h>
#include <raii/scope_guard.hpp>

void PrusaGcodeSuite::G750() {
    GCodeParser2 p;

    if (!p.parse_marlin_command()) {
        return;
    }

    auto target_x = p.option<float>('X');
    auto target_y = p.option<float>('Y');
    auto target_e = p.option<float>('E');
    auto fr = p.option<float>('F');

    if (!target_x && !target_y && !target_e) {
        return;
    }

    // Purge gcode sequences use relative E moves — save and restore the original mode
    const uint8_t saved_axis_relative = GcodeSuite::axis_relative;
    GcodeSuite::set_e_relative();
    ScopeGuard restore_e_mode = [saved_axis_relative] {
        GcodeSuite::axis_relative = saved_axis_relative;
    };

    // Subtract hotend offset to convert from physical cleaner position to native (carriage) coordinates,
    // so the nozzle tip ends up at the cleaner regardless of which tool is active.
    const auto offset = native_logical_offset();

    xyze_pos_t target = current_position;

    if (target_x) {
        target.x = *target_x + X_NOZZLE_CLEANER_ORIGIN + config_store().nozzle_cleaner_x_origin_offset.get() - offset.x;
    }
    if (target_y) {
        target.y = *target_y + Y_NOZZLE_CLEANER_ORIGIN + config_store().nozzle_cleaner_y_origin_offset.get() - offset.y;
    }
    do_blocking_move_to_xye(target.x, target.y, target_e.value_or(0), fr.value_or(PrusaToolChangerUtils::TRAVEL_MOVE_MM_S));
}
