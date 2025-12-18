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

#if ENABLED(Z_TRIPLE_ENDSTOPS)

#include "../gcode.h"

  #include "../../module/endstops.h"

  /**
   * M666: Set Triple Endstops offsets for Z.
   *       With no parameters report current offsets.
   *
   * For Triple Z Endstops:
   *   Set Z2 Only: M666 S2 Z<offset>
   *   Set Z3 Only: M666 S3 Z<offset>
   *      Set Both: M666 Z<offset>
   */
  void GcodeSuite::M666() {
    if (parser.seenval('Z')) {
      const float z_adj = parser.value_linear_units();
      const int ind = parser.intval('S');
      if (!ind || ind == 2) endstops.z2_endstop_adj = z_adj;
      if (!ind || ind == 3) endstops.z3_endstop_adj = z_adj;
    } else {
      SERIAL_ECHOPGM("Triple Endstop Adjustment (mm): ");
      SERIAL_ECHOPAIR(" Z2:", endstops.z2_endstop_adj);
      SERIAL_ECHOPAIR(" Z3:", endstops.z3_endstop_adj);
      SERIAL_EOL();
    }
  }

#endif
