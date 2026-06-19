#include "parking.hpp"

#include <Marlin/src/gcode/gcode.h>
#include <Marlin/src/module/motion.h>
#include <config_store/store_instance.hpp>
#include <utils/overloaded_visitor.hpp>
#include <module/planner.h>

#include <option/has_wastebin.h>
#include <option/has_wastebin_fill_tracking.h>

namespace mapi {

// Make sure our little [[no_unique_address]] trick works
static_assert(sizeof(ParkingPosition::AtLeast) == 8);

ParkingPosition get_parking_position(ParkPosition position) {
    switch (position) {
    case ParkPosition::park:
#if HAS_INDX()
        return apply_nozzle_cleaner_offset({ X_NOZZLE_PARK_POINT, Y_NOZZLE_PARK_POINT, mapi::ParkingPosition::AtLeast { .above_print = Z_NOZZLE_PARK_POINT } });
#else
        return ParkingPosition(XYZ_NOZZLE_PARK_POINT);
#endif
    case ParkPosition::purge: {
#if HAS_WASTEBIN()
    #if HAS_INDX()
        // Wastebin is fixed to the CoreXY gantry, Z does not matter
        static constexpr ParkingPosition base_pos { X_WASTEBIN_POINT, Y_WASTEBIN_POINT, mapi::ParkingPosition::AtLeast { .above_print = 2 } };
        return apply_nozzle_cleaner_offset(base_pos);

    #elif PRINTER_IS_PRUSA_iX()
        // Wastebin is fixed to the CoreXY gantry, Z does not matter
        return ParkingPosition { X_WASTEBIN_POINT, Y_WASTEBIN_POINT, mapi::ParkingPosition::AtLeast { .above_print = 2 } };

    #else
        #error Need to define wastebin parking position
    #endif
#else
        return ParkingPosition { X_AXIS_LOAD_POS, Y_AXIS_LOAD_POS, ParkingPosition::AtLeast { .above_print = Z_NOZZLE_PARK_RISE, .absolute = Z_AXIS_LOAD_POS } };
#endif
    }

    case ParkPosition::load:
        return ParkingPosition { X_AXIS_LOAD_POS, Y_AXIS_LOAD_POS, ParkingPosition::AtLeast { .above_print = Z_NOZZLE_PARK_RISE, .absolute = Z_AXIS_LOAD_POS } };

    case ParkPosition::unload:
        return ParkingPosition { X_AXIS_UNLOAD_POS, Y_AXIS_UNLOAD_POS, ParkingPosition::AtLeast { .above_print = Z_NOZZLE_PARK_RISE, .absolute = Z_AXIS_UNLOAD_POS } };

    case ParkPosition::loadcell_selftest:
        return ParkingPosition(XYZ_LOADCELL_SELFTEST_POINT);

#if HAS_WASTEBIN_FILL_TRACKING()
    case ParkPosition::empty_wastebin:
        // Park at the INDX home corner (X to the min endstop, Y all the way back), clear of the
        // cleaner which sits at high X, and drop the bed low (Z at least 160, or 20 above tall
        // prints) so there is room to reach in and empty the cleaner.
        return ParkingPosition { 0.0f, static_cast<float>(Y_MAX_POS), ParkingPosition::AtLeast { .above_print = 20, .absolute = 160 } };
#endif

    case ParkPosition::_cnt:
        bsod_unreachable();
    }
    bsod_unreachable();
}

xyz_bool_t ParkingPosition::axes_needing_homing() const {
    // We want XY homing requirements to be joined, mainly bcs of pre_park_move_pattern
    const bool do_axis_xy = std::holds_alternative<float>(x) || std::holds_alternative<float>(y);
    return xyz_bool_t {
        .x = do_axis_xy,
        .y = do_axis_xy,
        .z = std::holds_alternative<float>(z),
    };
}

xyz_pos_t ParkingPosition::to_xyz_pos(const xyz_pos_t &pos) const {
    const auto result = to_nan_xyz_pos(pos);
    if (std::isnan(result.x) || std::isnan(result.y) || std::isnan(result.z)) {
        bsod("Conversion to xyz_pos_t failed.");
    }
    return result;
}

float ParkingPosition::resolve_z(float reference_z) const {
    return match(
        z, //
        [&](Unchanged) { return reference_z; }, //
        [](float val) { return val; }, //
        [&](Relative arg) {
            return std::clamp<float>(reference_z + arg.delta, Z_MIN_POS, Z_MAX_POS);
        }, //
        [&](AtLeast arg) {
            float min_z = arg.absolute;

            if (!std::isnan(arg.above_print)) {
                min_z = std::max(min_z, planner.max_printed_z + arg.above_print);
            }

            // Don't let Z go down
            return std::clamp<float>(reference_z, min_z, Z_MAX_POS);
        } //
    );
}

xyz_pos_t ParkingPosition::to_nan_xyz_pos(const xyz_pos_t &pos) const {
    return {
        .x = std::holds_alternative<Unchanged>(x) ? pos.x : std::get<float>(x),
        .y = std::holds_alternative<Unchanged>(y) ? pos.y : std::get<float>(y),
        .z = resolve_z(pos.z),
    };
}

ParkingPosition ParkingPosition::from_xyz_pos(const xyz_pos_t &pos) {
    return ParkingPosition {
        std::isnan(pos.x) ? (X)unchanged : pos.x,
        std::isnan(pos.y) ? (Y)unchanged : pos.y,
        std::isnan(pos.z) ? (Z)unchanged : pos.z,
    };
}

ParkingPosition ParkingPosition::from_xy_relative_z_pos(const xyz_pos_t &pos) {
    return ParkingPosition {
        std::isnan(pos.x) ? (X)unchanged : pos.x,
        std::isnan(pos.y) ? (Y)unchanged : pos.y,
        std::isnan(pos.z) ? (Z)unchanged : Relative { pos.z },
    };
}

#if HAS_INDX()
/// Applies nozzle cleaner origin offsets (from calibration) to the given parking position's X and Y.
ParkingPosition apply_nozzle_cleaner_offset(const ParkingPosition &position) {
    const float x_offset = config_store().nozzle_cleaner_x_origin_offset.get();
    const float y_offset = config_store().nozzle_cleaner_y_origin_offset.get();

    ParkingPosition result = position;
    if (auto *x = std::get_if<float>(&result.x)) {
        *x += x_offset;
    }
    if (auto *y = std::get_if<float>(&result.y)) {
        *y += y_offset;
    }
    return result;
}
#endif

#if HAS_NOZZLE_CLEANER()
/**
 * Does the extra parking moves except the last one to move in the correct
 * directions and avoid the nozzle cleaner.
 *
 *  RearArea = WasteArea + NoWasteLand         │ X_NOZZLE_PARK_POINT
 *                                             ▼
 * ┌────────────────NO─WASTE─LAND────────────────┬─────────WASTE─AREA─────────┐
 * │                                         .  .│                            │
 * │                                         .  .│                        * ◄─┼────X,Y_WASTEBIN_POINT
 * │                                         .  .├ ─ ─ ─ ─ ─ ─ ┐              │
 * │                                         .  .│    ╲‾‾╲     |   ╱‾‾╱=>     │
 * │                                         ....│      ╲  ╲   |  |  |        │
 * │                                             │        ╲__╲ │   ╲ˍˍ╲=>     │
 * │                                             │    BRUSH    │              │
 * ├─────────────────────────────────────────────┴─────────────┴──────────────┤ ◄── Y_WASTEBIN_SAFE_POINT
 * │                                             X_BRUSH_POINT ▲              |
 * │   _____  _____  _____ _   _ _______            _____  ______             │
 * │  |  __ \|  __ \|_   _| \ | |__   __|     /\   |  __ \|  ____|   /\       │
 * │  | |__) | |__) | | | |  \| |  | |       /  \  | |__) | |__     /  \      │
 * │  |  ___/|  _  /  | | | . ` |  | |      / /\ \ |  _  /|  __|   / /\ \     │
 * │  | |    | | \ \ _| |_| |\  |  | |     / ____ \| | \ \| |____ / ____ \    │        ▲
 * │  |_|    |_|  \_\_____|_| \_|  |_|    /_/    \_\_|  \_\______/_/    \_\   │        | Y
 * │                                                                          │        |
 * │                                                                          │        |      X
 * └──────────────────────────────────────────────────────────────────────────┘        O ──────►
 */
static void pre_park_move_pattern(const feedRate_t &feedrate, const xy_pos_t &destination) {
    #if PRINTER_IS_PRUSA_iX()
    static constexpr float x_border_point = X_NOZZLE_PARK_POINT + 1;
    static constexpr float X_BRUSH_POINT = 242.f; // X point before which we can go directly from (no need to avoid v-blade)

    const bool start_in_rear_area = current_position.y > Y_WASTEBIN_SAFE_POINT;
    const bool destination_in_rear_area = destination.y > Y_WASTEBIN_SAFE_POINT;

    const bool start_in_wastebin_area = start_in_rear_area && current_position.x > x_border_point;
    const bool destination_in_wastebin_area = destination_in_rear_area && destination.x > x_border_point;

    const bool start_in_brush_area = start_in_wastebin_area && current_position.x < X_BRUSH_POINT;
    const bool destination_in_brush_area = destination_in_wastebin_area && destination.x < X_BRUSH_POINT;

    if (start_in_rear_area != destination_in_rear_area) { // One in the rear, other in print area
        if (start_in_wastebin_area || destination_in_wastebin_area) { // One in waste area, other in print area
            if (!start_in_brush_area && !destination_in_brush_area) { // If neither is in brush area, go around vblade
                do_blocking_move_to_x(X_WASTEBIN_POINT, feedrate);
            } // If we are in brush area, we can go directly
            do_blocking_move_to_y(destination.y, feedrate);
        } else { // One in no-waste land, other in print area
            if (start_in_rear_area) { // Start in no-waste land, end in print area
                do_blocking_move_to_y(destination.y, feedrate);
            } else { // Start in print area, end in no-waste land
                do_blocking_move_to_x(destination.x, feedrate);
            }
        }
    } else if (start_in_rear_area) { // Both in the rear area
        if (start_in_wastebin_area != destination_in_wastebin_area) { // One in waste area, other in no-waste land
            if (start_in_wastebin_area) { // Start in waste area, end in no-waste land
                if (!start_in_brush_area) { // If we don't start in brush, avoid v-blade
                    do_blocking_move_to_x(X_WASTEBIN_POINT, feedrate);
                }
                do_blocking_move_to_y(Y_WASTEBIN_SAFE_POINT, feedrate);
                do_blocking_move_to_x(destination.x, feedrate);
            } else { // Start in no-waste land, end in waste area
                if (!destination_in_brush_area) { // If we don't end in brush, avoid v-blade
                    do_blocking_move_to_x(X_WASTEBIN_POINT, feedrate);
                }
                do_blocking_move_to_x(X_WASTEBIN_POINT, feedrate);
                do_blocking_move_to_y(destination.y, feedrate);
            }
        } else if (start_in_wastebin_area) { // Both in waste area (here no need to worry about brush)
            do_blocking_move_to_x(destination.x, feedrate);
        } else { // Both in no-waste land
            do_blocking_move_to_y(Y_WASTEBIN_SAFE_POINT, feedrate);
            do_blocking_move_to_x(destination.x, feedrate);
        }
    } // Both in print area, no need to pre-park move
    #elif HAS_INDX()
    const bool start_in_side_area = current_position.x > X_WASTEBIN_SAFE_POINT;
    const bool destination_in_side_area = destination.x > X_WASTEBIN_SAFE_POINT;

    const bool start_in_wastebin_area = start_in_side_area && current_position.y > Y_WASTEBIN_SAFE_POINT;
    const bool destination_in_wastebin_area = destination_in_side_area && destination.y > Y_WASTEBIN_SAFE_POINT;

    if (start_in_side_area != destination_in_side_area) { // One in the side, other in print area
        if (start_in_wastebin_area || destination_in_wastebin_area) { // One in waste area, other in print area
            do_blocking_move_to_y(Y_BRUSH_AVOID_POINT, feedrate);
            do_blocking_move_to_x(destination.x, feedrate);
        } else { // One in no-waste land, other in print area
            if (start_in_side_area) { // Start in no-waste land, end in print area
                do_blocking_move_to_x(X_WASTEBIN_SAFE_POINT, feedrate);
            } else { // Start in print area, end in no-waste land
                do_blocking_move_to_xy(X_WASTEBIN_SAFE_POINT, destination.y, feedrate);
            }
        }
    } else if (start_in_side_area) { // Both in the side area
        if (start_in_wastebin_area != destination_in_wastebin_area) { // One in waste area, other in no-waste land
            if (start_in_wastebin_area) { // Start in waste area, end in no-waste land
                do_blocking_move_to_y(Y_BRUSH_AVOID_POINT, feedrate);
                do_blocking_move_to_x(X_WASTEBIN_SAFE_POINT, feedrate);
                do_blocking_move_to_y(destination.y, feedrate);
            } else { // Start in no-waste land, end in waste area
                do_blocking_move_to_x(X_WASTEBIN_SAFE_POINT, feedrate);
                do_blocking_move_to_y(Y_BRUSH_AVOID_POINT, feedrate);
                do_blocking_move_to_x(destination.x, feedrate);
            }
        }
    } // Both in print area, no need to pre-park move
    #else
        #error "Implement"
    #endif
}

void move_out_of_nozzle_cleaner_area() {
    #if PRINTER_IS_PRUSA_iX()
    pre_park_move_pattern(NOZZLE_PARK_XY_FEEDRATE, { X_WASTEBIN_POINT, Y_WASTEBIN_SAFE_POINT });
    #else
    pre_park_move_pattern(NOZZLE_PARK_XY_FEEDRATE, { X_WASTEBIN_SAFE_POINT, Y_BRUSH_AVOID_POINT });
    #endif
}
#endif

bool park(const ParkingPosition &parking_position) {
    static constexpr feedRate_t fr_xy = NOZZLE_PARK_XY_FEEDRATE, fr_z = NOZZLE_PARK_Z_FEEDRATE;

    const auto curr_pos = current_position.xyz();
    const auto park_destination = parking_position.to_xyz_pos(curr_pos);
    const auto needs_homed = parking_position.axes_needing_homing();

    // @returns false if the move cannot be performed safely (the caller aborts)
    const auto move_z = [&]() -> bool {
        if (park_destination.z == curr_pos.z) {
            return true;
        }
        if (axes_home_level.is_homed(Z_AXIS, AxisHomeLevel::imprecise)) {
            do_blocking_move_to_z(park_destination.z, fr_z);

        } else if (needs_homed.z) {
            // The move requires homed Z axis, which we don't have
            // Don't allow XY moves without lifting
            return false;

        } else {
            do_homing_move(Z_AXIS, park_destination.z - curr_pos.z, HOMING_FEEDRATE_INVERTED_Z);
        }
        return true;
    };

    const auto move_xy = [&]() -> bool {
        if (park_destination.xy() == curr_pos.xy()) {
            return true;
        }
        if (!axes_home_level.is_homed({ X_AXIS, Y_AXIS }, AxisHomeLevel::imprecise)) {
            // We need to always be homed to do XY moves
            return false;
        }

#if HAS_INDX()
        // move out of dock area perpendicularly before parking
        if (curr_pos.y < Y_DOCK_PARKING_MIN_SAFE_POS) {
            do_blocking_move_to_y(Y_DOCK_PARKING_MIN_SAFE_POS, feedrate_mm_s);
        }
#endif

#if HAS_NOZZLE_CLEANER()
        pre_park_move_pattern(fr_xy, park_destination.xy());
#endif
        do_blocking_move_to_xy(park_destination, fr_xy);
        return true;
    };

    // Order the two moves so the nozzle is never dragged across the print: the XY traverse always
    // happens at the higher of the current/destination Z. Going up: lift first, then traverse.
    // Going down: traverse first (at the current, higher Z), then descend. Each move handles its own
    // homing requirements internally (and bails out if unsatisfied).
    if (park_destination.z < curr_pos.z) {
        if (!move_xy() || !move_z()) {
            return false;
        }
    } else {
        if (!move_z() || !move_xy()) {
            return false;
        }
    }

    planner.synchronize();
    report_current_position();
    return !planner.draining();
}

void home_if_needed_and_park(const ParkingPosition &parking_position) {
    // We only need homing if we move to absolute coordinates
    // For relative moves we can use do_homing_move
    const xyz_bool_t do_axis = parking_position.axes_needing_homing();
    GcodeSuite::G28_no_parser(do_axis.x, do_axis.y, do_axis.z,
        {
            .only_if_needed = true,
            .z_raise = 3,
            .precise = false, // We don't need precise position for parking
        });

    park(parking_position);
}

} // namespace mapi
