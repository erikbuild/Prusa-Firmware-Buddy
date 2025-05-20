/**
 * Marlin 3D Printer Firmware
 * Copyright (c) 2019 MarlinFirmware [https://github.com/MarlinFirmware/Marlin]
 *
 * Based on Sprinter and grbl.
 * Copyright (c) 2011 Camiel Gubbels / Erik van der Zalm
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "../inc/MarlinConfigPre.h"

#include "tool_change.h"

#include "probe.h"
#include "motion.h"
#include "planner.h"
#include "temperature.h"

#include "../Marlin.h"

#define DEBUG_OUT ENABLED(DEBUG_LEVELING_FEATURE)
#include "../core/debug_out.h"

#if EXTRUDERS > 1
  toolchange_settings_t toolchange_settings;  // Initialized by settings.load()
#endif

#if ENABLED(SINGLENOZZLE)
  uint16_t singlenozzle_temp[EXTRUDERS];
  #if FAN_COUNT > 0
    uint8_t singlenozzle_fan_speed[EXTRUDERS];
  #endif
#endif

#if ANY(SWITCHING_EXTRUDER, SWITCHING_NOZZLE)
  #include "servo.h"
#endif

#if ENABLED(EXT_SOLENOID)
  #include "../feature/solenoid.h"
#endif

#if ENABLED(MK2_MULTIPLEXER)
  #include "../feature/snmm.h"
#endif

#if HAS_LEVELING
  #include "../feature/bedlevel/bedlevel.h"
#endif

#if HAS_FANMUX
  #include "../feature/fanmux.h"
#endif

#if ENABLED(PRUSA_MMU2)
  #include "../feature/prusa/MMU2/mmu2_mk4.h"
#endif

#if ENABLED(ADVANCED_PAUSE_FEATURE)
  #include "../feature/pause.h"
#endif

#if DO_SWITCH_EXTRUDER

  #if EXTRUDERS > 3
    #define _SERVO_NR(E) ((E) < 2 ? SWITCHING_EXTRUDER_SERVO_NR : SWITCHING_EXTRUDER_E23_SERVO_NR)
  #else
    #define _SERVO_NR(E) SWITCHING_EXTRUDER_SERVO_NR
  #endif

  void move_extruder_servo(const uint8_t e) {
    planner.synchronize();
    #if EXTRUDERS & 1
      if (e < EXTRUDERS - 1)
    #endif
    {
      MOVE_SERVO(_SERVO_NR(e), servo_angles[_SERVO_NR(e)][e]);
      safe_delay(500);
    }
  }

#endif // DO_SWITCH_EXTRUDER

#if ENABLED(SWITCHING_NOZZLE)

  #if SWITCHING_NOZZLE_TWO_SERVOS

    inline void _move_nozzle_servo(const uint8_t e, const uint8_t angle_index) {
      constexpr int8_t  sns_index[2] = { SWITCHING_NOZZLE_SERVO_NR, SWITCHING_NOZZLE_E1_SERVO_NR };
      constexpr int16_t sns_angles[2] = SWITCHING_NOZZLE_SERVO_ANGLES;
      planner.synchronize();
      MOVE_SERVO(sns_index[e], sns_angles[angle_index]);
      safe_delay(500);
    }

    void lower_nozzle(const uint8_t e) { _move_nozzle_servo(e, 0); }
    void raise_nozzle(const uint8_t e) { _move_nozzle_servo(e, 1); }

  #else

    void move_nozzle_servo(const uint8_t angle_index) {
      planner.synchronize();
      MOVE_SERVO(SWITCHING_NOZZLE_SERVO_NR, servo_angles[SWITCHING_NOZZLE_SERVO_NR][angle_index]);
      safe_delay(500);
    }

  #endif

#endif // SWITCHING_NOZZLE

inline void _line_to_current(const AxisEnum fr_axis, const float fscale=1) {
  line_to_current_position(planner.settings.max_feedrate_mm_s[fr_axis] * fscale);
}
inline void slow_line_to_current(const AxisEnum fr_axis) { _line_to_current(fr_axis, 0.5f); }
inline void fast_line_to_current(const AxisEnum fr_axis) { _line_to_current(fr_axis); }

#if EXTRUDERS
  inline void invalid_extruder_error(const uint8_t e) {
    SERIAL_ECHO_START();
    SERIAL_CHAR('T'); SERIAL_ECHO(int(e));
    SERIAL_CHAR(' '); SERIAL_ECHOLNPGM(MSG_INVALID_EXTRUDER);
  }
#endif

/**
 * Perform a tool-change, which may result in moving the
 * previous tool out of the way and the new tool into place.
 */
void tool_change(const uint8_t new_tool,
                 tool_return_t return_type /*= tool_return_t::to_current*/,
                 tool_change_lift_t z_lift /*= tool_change_lift_t::full_lift*/,
                 bool z_return /*= true*/){

  #if ENABLED(PRUSA_MMU2)

    UNUSED(return_type);

    MMU2::mmu2.tool_change(new_tool);

  #elif EXTRUDERS == 0

    // Nothing to do
    UNUSED(new_tool); UNUSED(return_type);

  #elif EXTRUDERS < 2

    UNUSED(return_type);

    if (new_tool) invalid_extruder_error(new_tool);
    return;

  #else // EXTRUDERS > 1

    planner.synchronize();

    #if ENABLED(DUAL_X_CARRIAGE)  // Only T0 allowed if the Printer is in DXC_DUPLICATION_MODE or DXC_MIRRORED_MODE
      if (new_tool != 0 && dxc_is_duplicating())
         return invalid_extruder_error(new_tool);
    #endif

    if (new_tool >= EXTRUDERS)
      return invalid_extruder_error(new_tool);

    if ((return_type > tool_return_t::no_return) && !all_axes_homed()) {
      return_type = tool_return_t::no_return;
      if (DEBUGGING(LEVELING)) DEBUG_ECHOLNPGM("No move (not homed)");
    }

    #if ENABLED(DUAL_X_CARRIAGE)
      const bool idex_full_control = dual_x_carriage_mode == DXC_FULL_CONTROL_MODE;
    #else
      constexpr bool idex_full_control = false;
    #endif

    const uint8_t old_tool = active_extruder;
    const bool can_move_away = (return_type > tool_return_t::no_return) && !idex_full_control;

    // calculate where we should return to
    auto return_position = destination;
    if (return_type == tool_return_t::to_current) {
      if (all_axes_known())
        return_position = current_position;
      else
        return_type = tool_return_t::no_return;
    }
    float return_delta_z = return_position.z - current_position.z;

    #if ENABLED(TOOLCHANGE_FILAMENT_SWAP)
      const bool should_swap = can_move_away && toolchange_settings.swap_length;
      #if ENABLED(PREVENT_COLD_EXTRUSION)
        const bool too_cold = !DEBUGGING(DRYRUN) && (thermalManager.targetTooColdToExtrude(old_tool) || thermalManager.targetTooColdToExtrude(new_tool));
      #else
        constexpr bool too_cold = false;
      #endif
      if (should_swap) {
        if (too_cold) {
          SERIAL_ECHO_MSG(MSG_ERR_HOTEND_TOO_COLD);
          #if ENABLED(SINGLENOZZLE)
            active_extruder = new_tool;
            return;
          #endif
        }
        else {
          #if ENABLED(ADVANCED_PAUSE_FEATURE)
            do_pause_e_move(-toolchange_settings.swap_length, MMM_TO_MMS(toolchange_settings.retract_speed));
          #else
            current_position.e -= toolchange_settings.swap_length / planner.e_factor[old_tool];
            planner.buffer_line(current_position, MMM_TO_MMS(toolchange_settings.retract_speed), old_tool);
            planner.synchronize();
          #endif
        }
      }
    #endif // TOOLCHANGE_FILAMENT_SWAP

    if (new_tool != old_tool) {
      #if HAS_LEVELING
        // Set current position to the physical position
        TEMPORARY_BED_LEVELING_STATE(false);
      #endif

      #if SWITCHING_NOZZLE_TWO_SERVOS
        raise_nozzle(old_tool);
      #endif

      REMEMBER(fr, feedrate_mm_s, XY_PROBE_FEEDRATE_MM_S);

      #if HAS_SOFTWARE_ENDSTOPS
        #if HAS_HOTEND_OFFSET
          #define _EXT_ARGS , old_tool, new_tool
        #else
          #define _EXT_ARGS
        #endif
        update_software_endstops(X_AXIS _EXT_ARGS);
        #if DISABLED(DUAL_X_CARRIAGE)
          update_software_endstops(Y_AXIS _EXT_ARGS);
          update_software_endstops(Z_AXIS _EXT_ARGS);
        #endif
      #endif

      #if DISABLED(SWITCHING_NOZZLE)
        if (can_move_away) {
          // Do a small lift to avoid the workpiece for parking
          current_position.z += toolchange_settings.z_raise;
          if (return_type > tool_return_t::no_return && return_delta_z > 0) {
            // also immediately account for clearance in the return move
            // TODO: this might not cover the entire plane as MBL is turned off!
            current_position.z += return_delta_z;
          }

          #if HAS_SOFTWARE_ENDSTOPS
            NOMORE(current_position.z, soft_endstop.max.z);
          #endif
          fast_line_to_current(Z_AXIS);
          #if ENABLED(TOOLCHANGE_PARK)
            current_position = toolchange_settings.change_point;
          #endif
          planner.buffer_line(current_position, feedrate_mm_s, old_tool);
          planner.synchronize();
        }
      #endif

      #if HAS_HOTEND_OFFSET
        xyz_pos_t diff = hotend_offset[new_tool] - hotend_currently_applied_offset;
        #if ENABLED(DUAL_X_CARRIAGE)
          diff.x = 0;
        #endif
      #endif

      #if ENABLED(DUAL_X_CARRIAGE)
        dualx_tool_change(new_tool, no_move);
      #elif ENABLED(PARKING_EXTRUDER)                                   // Dual Parking extruder
        parking_extruder_tool_change(new_tool, no_move);
      #elif ENABLED(MAGNETIC_PARKING_EXTRUDER)                          // Magnetic Parking extruder
        magnetic_parking_extruder_tool_change(new_tool);
      #elif ENABLED(SWITCHING_TOOLHEAD)                                 // Switching Toolhead
        switching_toolhead_tool_change(new_tool, no_move);
      #elif ENABLED(MAGNETIC_SWITCHING_TOOLHEAD)                        // Magnetic Switching Toolhead
        magnetic_switching_toolhead_tool_change(new_tool, no_move);
      #elif ENABLED(ELECTROMAGNETIC_SWITCHING_TOOLHEAD)                 // Magnetic Switching ToolChanger
        em_switching_toolhead_tool_change(new_tool, no_move);
      #elif ENABLED(SWITCHING_NOZZLE) && !SWITCHING_NOZZLE_TWO_SERVOS   // Switching Nozzle (single servo)
        // Raise by a configured distance to avoid workpiece, except with
        // SWITCHING_NOZZLE_TWO_SERVOS, as both nozzles will lift instead.
        current_position.z += _MAX(-diff.z, 0.0) + toolchange_settings.z_raise;
        #if HAS_SOFTWARE_ENDSTOPS
          NOMORE(current_position.z, soft_endstop.max.z);
        #endif
        if (!no_move) fast_line_to_current(Z_AXIS);
        move_nozzle_servo(new_tool);
      #endif

      #if DISABLED(DUAL_X_CARRIAGE)
        active_extruder = new_tool; // Set the new active extruder
      #endif

      // The newly-selected extruder XYZ is actually at...
      if (DEBUGGING(LEVELING)) DEBUG_ECHOLNPAIR("Offset Tool XY by { ", diff.x, ", ", diff.y, ", ", diff.z, " }");
      current_position += diff;

      #if HAS_HOTEND_OFFSET
        hotend_currently_applied_offset += diff;
      #endif

      // Tell the planner the new "current position"
      sync_plan_position();

      #if ENABLED(DELTA)
        //LOOP_XYZ(i) update_software_endstops(i); // or modify the constrain function
        const bool safe_to_move = current_position.z < delta_clip_start_height - 1;
      #else
        constexpr bool safe_to_move = true;
      #endif

      // Return to position and lower again
      if (safe_to_move && IsRunning()) {

        #if ENABLED(SINGLENOZZLE)
          #if FAN_COUNT > 0
            singlenozzle_fan_speed[old_tool] = thermalManager.fan_speed[0];
            thermalManager.fan_speed[0] = singlenozzle_fan_speed[new_tool];
          #endif

          singlenozzle_temp[old_tool] = thermalManager.temp_hotend[0].target;
          if (singlenozzle_temp[new_tool] && singlenozzle_temp[new_tool] != singlenozzle_temp[old_tool]) {
            thermalManager.setTargetHotend(singlenozzle_temp[new_tool], 0);
            (void)thermalManager.wait_for_hotend(0, false);  // Wait for heating or cooling
          }
        #endif

        #if ENABLED(TOOLCHANGE_FILAMENT_SWAP)
          if (should_swap && !too_cold) {
            #if ENABLED(ADVANCED_PAUSE_FEATURE)
              do_pause_e_move(toolchange_settings.swap_length, MMM_TO_MMS(toolchange_settings.prime_speed));
              do_pause_e_move(toolchange_settings.extra_prime, ADVANCED_PAUSE_PURGE_FEEDRATE);
            #else
              current_position.e += toolchange_settings.swap_length / planner.e_factor[new_tool];
              planner.buffer_line(current_position, MMM_TO_MMS(toolchange_settings.prime_speed), new_tool);
              current_position.e += toolchange_settings.extra_prime / planner.e_factor[new_tool];
              planner.buffer_line(current_position, MMM_TO_MMS(toolchange_settings.prime_speed * 0.2f), new_tool);
            #endif
            planner.synchronize();
            planner.set_e_position_mm((return_position.e = current_position.e = current_position.e - (TOOLCHANGE_FIL_EXTRA_PRIME)));
          }
        #endif

        // Prevent a move outside physical bounds
        #if ENABLED(MAGNETIC_SWITCHING_TOOLHEAD)
          // If the original position is within tool store area, go to X origin at once
          if (return_position.y < SWITCHING_TOOLHEAD_Y_POS + SWITCHING_TOOLHEAD_Y_CLEAR) {
            current_position.x = 0;
            planner.buffer_line(current_position, planner.settings.max_feedrate_mm_s[X_AXIS], new_tool);
            planner.synchronize();
          }
        #else
          apply_motion_limits(return_position);
        #endif

        // Should the nozzle move back to the old position?
        if ((return_type > tool_return_t::no_return) && all_axes_known()) {
          #if ENABLED(TOOLCHANGE_NO_RETURN)
            // Just move back down
            if (DEBUGGING(LEVELING)) DEBUG_ECHOLNPGM("Move back Z only");
            do_blocking_move_to_z(return_position.z, planner.settings.max_feedrate_mm_s[Z_AXIS]);
          #else
            // Move back to the original (or adjusted) position
            if (DEBUGGING(LEVELING)) DEBUG_POS("Move back", return_position);

            // move across the XY plane
            current_position.set(return_position.x, return_position.y);
            planner.buffer_line(current_position, feedrate_mm_s, new_tool);
          #endif
        }
        else if (DEBUGGING(LEVELING)) DEBUG_ECHOLNPGM("Move back skipped");

        #if ENABLED(DUAL_X_CARRIAGE)
          active_extruder_parked = false;
        #endif
      }
      #if ENABLED(SWITCHING_NOZZLE)
        else {
          // Move back down. (Including when the new tool is higher.)
          do_blocking_move_to_z(return_position.z, planner.settings.max_feedrate_mm_s[Z_AXIS]);
        }
      #endif

      #if ENABLED(PRUSA_MMU2)
        mmu2.tool_change(new_tool);
      #endif

      #if SWITCHING_NOZZLE_TWO_SERVOS
        lower_nozzle(new_tool);
      #endif

    } // (new_tool != old_tool)

    // Finally move to return_position if possible/wanted
    if ((return_type > tool_return_t::no_return) && all_axes_known() && current_position != return_position) {
      destination = return_position;
      prepare_move_to_destination();
    }

    #if ENABLED(EXT_SOLENOID) && DISABLED(PARKING_EXTRUDER)
      disable_all_solenoids();
      enable_solenoid_on_active_extruder();
    #endif

    #if ENABLED(MK2_MULTIPLEXER)
      if (new_tool >= E_STEPPERS) return invalid_extruder_error(new_tool);
      select_multiplexed_stepper(new_tool);
    #endif

    #if DO_SWITCH_EXTRUDER
      planner.synchronize();
      move_extruder_servo(active_extruder);
    #endif

    #if HAS_FANMUX
      fanmux_switch(active_extruder);
    #endif

    SERIAL_ECHO_START();
    SERIAL_ECHOLNPAIR(MSG_ACTIVE_EXTRUDER, int(active_extruder));

  #endif // EXTRUDERS > 1
}
