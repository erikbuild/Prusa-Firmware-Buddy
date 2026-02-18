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

#include "../../inc/MarlinConfig.h"

#if HAS_LEVELING

#include "bedlevel.h"
#include "../../module/planner.h"

#if ENABLED(LCD_BED_LEVELING)
  #include "../../lcd/ultralcd.h"
#endif

#if ENABLED(EXTENSIBLE_UI)
  #include "../../lcd/extensible_ui/ui_api.h"
#endif

bool leveling_is_valid() {
  return
    #if ENABLED(AUTO_BED_LEVELING_UBL)
      ubl.mesh_is_valid()
    #else // 3POINT, LINEAR
      true
    #endif
  ;
}

/**
 * Turn bed leveling on or off, fixing the current
 * position as-needed.
 *
 * Updates current_position to match the machine position given according to the modifiers change
 */
void set_bed_leveling_enabled(const bool enable/*=true*/) {
  if(enable == planner.leveling_active) {
    return;
  }

  planner.synchronize();

  const auto orig_machine_position = current_machine_position();

  planner.leveling_active = enable;

  // When changing MBL enable, the actual machine position stays the same, because the printer does not move
  // So we have to update the current_position to match the machine position
  set_current_position(to_native_pos(orig_machine_position));

  /// Update planner::machine_position as well to get rid of any imprecisions
  sync_plan_position();
}

TemporaryBedLevelingState::TemporaryBedLevelingState(const bool enable) : saved(planner.leveling_active) {
  set_bed_leveling_enabled(enable);
}

#if ENABLED(ENABLE_LEVELING_FADE_HEIGHT)

  void set_z_fade_height(const float zfh, const bool do_report/*=true*/) {

    if (planner.z_fade_height == zfh) return;

    const bool leveling_was_active = planner.leveling_active;
    set_bed_leveling_enabled(false);

    planner.set_z_fade_height(zfh);

    if (leveling_was_active) {
      const xyz_pos_t oldpos = current_position;
      set_bed_leveling_enabled(true);
      if (do_report && oldpos != current_position)
        report_current_position();
    }
  }

#endif // ENABLE_LEVELING_FADE_HEIGHT

/**
 * Reset calibration results to zero.
 */
void reset_bed_level() {
  #if ENABLED(AUTO_BED_LEVELING_UBL)
    ubl.reset();
  #else
    set_bed_leveling_enabled(false);
  #endif
}

#endif // HAS_LEVELING
