/**
 * Marlin 3D Printer Firmware
 * Copyright (c) 2020 MarlinFirmware [https://github.com/MarlinFirmware/Marlin]
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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "../../inc/MarlinConfig.h"

#if ENABLED(BABYSTEPPING)

#include "../gcode.h"
#include "../../feature/babystep.h"
#include "../../module/probe.h"
#include "../../module/planner.h"

/** \addtogroup G-Codes
 * @{
 */

/**
 *### M290: Babystepping <a href="https://reprap.org/wiki/G-code#M290:_Babystepping">M290: Babystepping</a>
 *
 *#### Usage
 *
 *    M290 [ S | X | Y | Z | R | P ]
 *
 *#### Parameters
 *
 * - `S` - Alias for Z
 * - `X` - A distance on the X axis
 * - `Y` - A distance on the Y axis
 * - `Z` - A distance on the Z axis
 * - `R` - Read
 * - `P` - Don't adjust the Z probe offset
 */
void GcodeSuite::M290() {
  #if ENABLED(BABYSTEP_XY)
    LOOP_NUM_AXES(a)
      if (parser.seenval(AXIS_CHAR(a)) || (a == Z_AXIS && parser.seenval('S'))) {
        const float offs = constrain(parser.value_axis_units((AxisEnum)a), -2, 2);
        babystep.add_mm((AxisEnum)a, offs);
      }
  #else
    if (parser.seenval('Z') || parser.seenval('S')) {
      const float offs = constrain(parser.value_axis_units(Z_AXIS), -2, 2);
      babystep.add_mm(Z_AXIS, offs);
    }
  #endif
}

#if ENABLED(EP_BABYSTEPPING) && DISABLED(EMERGENCY_PARSER)
  // Without Emergency Parser M293/M294 will be added to the queue
  void GcodeSuite::M293() { babystep.z_up(); }
  void GcodeSuite::M294() { babystep.z_down(); }
#endif

/** @}*/

#endif // BABYSTEPPING
