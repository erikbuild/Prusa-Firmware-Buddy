#include "PrusaGcodeSuite.hpp"

#include <config_store/store_instance.hpp>
#include <gcode/gcode_parser.hpp>
#include <module/prusa/toolchanger_utils.h>
#include <raii/scope_guard.hpp>
#include <module/planner.h>

void PrusaGcodeSuite::G750() {
    GCodeParser2 p;
    if (!p.parse_marlin_command()) {
        return;
    }

    MachinePosXYZE target = current_machine_position();

    if (auto x = p.option<float>('X')) {
        target.x = *x + X_NOZZLE_CLEANER_ORIGIN + config_store().nozzle_cleaner_x_origin_offset.get();
    }
    if (auto y = p.option<float>('Y')) {
        target.y = *y + Y_NOZZLE_CLEANER_ORIGIN + config_store().nozzle_cleaner_y_origin_offset.get();
    }
    if (auto e = p.option<float>('E')) {
        target.e += *e;
    }

    // Use machine coordinates - wastebin is outside of MBL area, applying MBL would do funny stuff.
    line_to_machine_pos(target, p.option<float>('F').value_or(PrusaToolChangerUtils::TRAVEL_MOVE_MM_S), { .ignore_e_factor = true });
    planner.synchronize();
}
