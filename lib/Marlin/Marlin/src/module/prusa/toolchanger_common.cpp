/// @file
#include "toolchanger.h"

#include <module/planner.h>

void PrusaToolChanger::z_shift(const float diff) {
    if (axes_home_level.is_homed(Z_AXIS, AxisHomeLevel::imprecise)) {
        MachinePosXYZE target = current_machine_position();
        target.z += diff;
        target.z = std::clamp<float>(target.z, Z_MIN_POS, Z_MAX_POS);
        line_to_machine_pos(target, Z_HOP_FEEDRATE_MM_S);

    } else {
        // Prevent nasty motor skipping
        do_homing_move(Z_AXIS, diff, HOMING_FEEDRATE_INVERTED_Z);
    }

    planner.synchronize();
}

float PrusaToolChanger::calc_z_raise(tool_return_t return_type, xyz_pos_t return_position, tool_change_lift_t z_lift, bool levelling_active) const {
    if (z_lift == tool_change_lift_t::no_lift) {
        return 0.f;
    }
    float z_raise = 0;
    if (return_type != tool_return_t::no_return) {
        float min_z = current_position.z;
        min_z = std::max(min_z, return_position.z); // account for clearance in the return move
        min_z = std::max(min_z, planner.max_printed_z); // raise above the printed model
        z_raise += (min_z - current_position.z);
    }
    if (z_lift == tool_change_lift_t::full_lift) {
        z_raise += toolchange_settings.z_raise; // avoid the workpiece for parking
    }
    if (levelling_active) {
        z_raise += get_mbl_z_lift_height();
    }
    return z_raise;
}
