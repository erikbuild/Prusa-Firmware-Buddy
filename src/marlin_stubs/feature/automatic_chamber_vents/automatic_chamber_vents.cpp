/// @file
#include "automatic_chamber_vents.hpp"

#include <feature/print_status_message/print_status_message_guard.hpp>
#include <mapi/parking.hpp>
#include <Marlin/src/gcode/gcode.h>
#include <Marlin/src/module/motion.h>
#include <Marlin/src/module/planner.h>
#include <printers.h>
#include <option/has_indx.h>
#include <raii/scope_guard.hpp>
#include <marlin_server.hpp>

#include <feature/chamber/chamber.hpp>

#if HAS_INDX()
    #include <module/prusa/toolchanger.h>
#endif

namespace automatic_chamber_vents {
namespace {

    /// The following constants define key positions for controlling a vent lever.
    /// These coordinates are absolute in the printer's coordinate system.
#if HAS_INDX()
    /// Dock whose tool's bowden operates the vent lever. Must be empty during the sequence.
    static constexpr auto VENT_DOCK = PhysicalToolIndex::from_raw(3); // T4
    static constexpr auto vent_feedrate = feedRate_t(60.0f); ///< 60 mm/s (G0 F3600)
    static constexpr float vent_travel_accel_mm_s2 = 1000.0f; ///< M204 T1000
    static constexpr auto DOCK_FRONT_X = 108.0f; ///< X of dock 4 — sequences assume they start aligned here
    static constexpr auto DOCK_FRONT_Y = 0.f; ///< Safe Y line in front of docks
#elif PRINTER_IS_PRUSA_COREONE()
    static constexpr auto Y_SAFE = -3.f; ///< Safe Y line with no risk of coming in contact with lever
    static constexpr auto Y_LEVER = -18.f; ///< In line with the lever
    static constexpr auto X_OPEN_START_POS = 37.f; ///< An X-axis position to the right of the lever, where opening move starts.
    static constexpr auto X_OPEN_END_POS = 24.f; ///< The X-axis position to move to on Y_LEVER to open the vents.
    static constexpr auto X_CLOSE_START_POS = 11.f; ///< An X-axis position to the left of the lever, where closing move starts.
    static constexpr auto X_CLOSE_END_POS = 26.f; ///< The X-axis position to move to on Y_LEVER to close the vents.
    static constexpr auto X_LEVER_MOVE_AWAY = 4.f; ///< The X-axis distance to move away from the lever after switch
    static constexpr auto lever_move_feedrate = feedRate_t(40.0f);
#elif PRINTER_IS_PRUSA_COREONEL()
    static constexpr auto Y_SAFE = 10.f; ///< Safe Y line with no risk of coming in contact with lever
    static constexpr auto Y_LEVER = -7.f; ///< In line with the lever
    static constexpr auto X_OPEN_START_POS = 25.f; ///< An X-axis position to the left of the lever, where opening move starts.
    static constexpr auto X_OPEN_END_POS = 42.f; ///< The X-axis position to move to on Y_LEVER to open the vents.
    static constexpr auto X_CLOSE_START_POS = 50.f; ///< An X-axis position to the right of the lever, where closing move starts.
    static constexpr auto X_CLOSE_END_POS = 35.f; ///< The X-axis position to move to on Y_LEVER to close the vents.
    static constexpr auto X_LEVER_MOVE_AWAY = -4.f; ///< The X-axis distance to move away from the lever after switch (COREONEL has inverted open/close direction)
    static constexpr auto lever_move_feedrate = feedRate_t(17.0f);
#else
    #error
#endif

    /// @brief Plans a move to a new X-axis coordinate.
    /// @param x The target X-axis position.
    /// @param feedrate The speed of the move in mm/s.
    void plan_to_x(float x, feedRate_t feedrate = feedRate_t(XY_PROBE_FEEDRATE_MM_S)) {
        xyze_pos_t xyz = current_position;
        xyz.x = x;
        prepare_move_to(xyz, feedrate, {});
    }

    /// @brief Plans a move to a new Y-axis coordinate.
    /// @param y The target Y-axis position.
    /// @param feedrate The speed of the move in mm/s.
    void plan_to_y(float y, feedRate_t feedrate = feedRate_t(XY_PROBE_FEEDRATE_MM_S)) {
        xyze_pos_t xyz = current_position;
        xyz.y = y;
        prepare_move_to(xyz, feedrate, {});
    }

    /// @brief Plans a move to a new XY coordinate.
    [[maybe_unused]] void plan_to_xy(float x, float y, feedRate_t feedrate = feedRate_t(XY_PROBE_FEEDRATE_MM_S)) {
        xyze_pos_t xyz = current_position;
        xyz.x = x;
        xyz.y = y;
        prepare_move_to(xyz, feedrate, {});
    }

#if HAS_INDX()
    /// @brief Ensure some tool is picked and dock 4 is empty, so the head can push the vent lever.
    /// @return true if a suitable tool is picked and dock 4 is confirmed empty.
    bool prepare_tool_for_vent_control() {
        // Prefer tool 4 if enabled — picking it definitionally empties dock 4.
        if (prusa_toolchanger.is_tool_enabled(VENT_DOCK)) {
            if (prusa_toolchanger.tool_change(VENT_DOCK, tool_return_t::no_return, {}, tool_change_lift_t::full_lift, false)) {
                return true;
            }
        }

        // Otherwise pick any enabled tool and verify dock 4 is empty.
        if (!prusa_toolchanger.pick_any_tool(tool_return_t::no_return, {}, tool_change_lift_t::full_lift, false)) {
            return false;
        }
        auto bump_res = prusa_toolchanger.bump_to_dock(VENT_DOCK);
        if (!bump_res.has_value() && bump_res.error() == PrusaToolChanger::BumpError::hit) {
            fatal_error(ErrCode::ERR_MECHANICAL_UNEXPECTED_TOOL);
            return false;
        }
        return true;
    }

    void open_vents_move_sequence() {
        plan_to_y(0.0f, vent_feedrate);
        plan_to_x(123.0f, vent_feedrate);
        plan_to_y(-7.0f, vent_feedrate);
        plan_to_xy(120.0f, -7.5f, vent_feedrate);
        plan_to_x(115.0f, vent_feedrate);
        plan_to_x(117.0f, vent_feedrate);
        plan_to_y(0.0f, vent_feedrate);
    }

    void close_vents_move_sequence() {
        plan_to_y(-11.5f, vent_feedrate);
        plan_to_x(117.0f, vent_feedrate);
        plan_to_x(115.0f, vent_feedrate);
        plan_to_y(0.0f, vent_feedrate);
    }

    /// @brief Run the full INDX vent-lever actuation: tool prep, reduced accel, move sequence.
    /// @return true on success.
    bool execute_indx_vent_lever(bool open) {
        if (!prepare_tool_for_vent_control()) {
            return false;
        }

        // Approach dock 4 at default travel speed — Y to the safe front-of-docks line first,
        // then X, so we don't slowly cross other docks during the reduced-accel sequence.
        plan_to_y(DOCK_FRONT_Y);
        plan_to_x(DOCK_FRONT_X);

        // Reduce travel acceleration for the lever interaction.
        const auto saved_travel_acceleration = planner.settings.travel_acceleration;
        auto apply_accel = [](float v) {
            auto s = planner.user_settings;
            s.travel_acceleration = v;
            planner.apply_settings(s);
        };
        apply_accel(vent_travel_accel_mm_s2);
        ScopeGuard restore_accel = [&] { apply_accel(saved_travel_acceleration); };

        if (open) {
            open_vents_move_sequence();
        } else {
            close_vents_move_sequence();
        }
        return true;
    }
#endif

}; // namespace

bool execute_control(VentState target_state) {
    const bool open = (target_state == VentState::open);

    PrintStatusMessageGuard psm_guard;
    if (open) {
        psm_guard.update<PrintStatusMessage::Type::opening_chamber_vents>({});
    } else {
        psm_guard.update<PrintStatusMessage::Type::closing_chamber_vents>({});
    }
    marlin_server::FSM_Holder fsm_holder { PhaseWait::print_status_message };

    // On INDX, vent control holds specific tool, for that we need pickup (that needs precise homing), this precise homing avoids double homing on the next pickup
    if (!GcodeSuite::G28_no_parser(true, true, false, { .only_if_needed = true, .precise = HAS_INDX() })) {
        return false;
    }

#if HAS_INDX()
    if (!execute_indx_vent_lever(open)) {
        return false;
    }
#else
    // Move to a safe Y-axis position to avoid the lever.
    plan_to_y(Y_SAFE);
    // Move to a horizontal position (left or right) of the lever.
    plan_to_x(open ? X_OPEN_START_POS : X_CLOSE_START_POS);
    // Move into the lever's Y-axis line.
    plan_to_y(Y_LEVER);
    // Move horizontally to engage the lever and switch it.
    plan_to_x(open ? X_OPEN_END_POS : X_CLOSE_END_POS, lever_move_feedrate);
    // Move horizontally to release the lever tension
    plan_to_x(open ? X_OPEN_END_POS + X_LEVER_MOVE_AWAY : X_CLOSE_END_POS - X_LEVER_MOVE_AWAY, lever_move_feedrate);
    // Back out to the safe Y-axis position to avoid a collision on future moves.
    plan_to_y(Y_SAFE);

    // Return to the home position after the vent operation.
    mapi::park(mapi::ZAction::no_move, mapi::get_parking_position(mapi::ParkPosition::park));
#endif

    // Wait for all planned moves to complete
    planner.synchronize();

    buddy::chamber().set_vent_state(target_state);

    return true;
}

} // namespace automatic_chamber_vents
