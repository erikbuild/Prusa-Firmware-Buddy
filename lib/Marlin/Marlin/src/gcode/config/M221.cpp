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

#include "../gcode.h"
#include "../../module/planner.h"
#include <utils/variant_utils.hpp>

/** \addtogroup G-Codes
 * @{
 */

/**
 *### M221: Get/Set extrusion percentage <a href="https://reprap.org/wiki/G-code#M221:_Set_extrude_factor_override_percentage">M221: Set extrude factor override percentage</a>
 *
 *#### Usage
 *
 *    M221 [ S | T ]
 *
 *#### Parameters
 *
 * - `S` - Flow percentage
 * - `T` - Tool
 *
 * Without parameters prints the current extrusion percentage
 */
void GcodeSuite::M221() {

  const std::optional<VirtualToolIndex> virtual_tool_opt = stdext::get_optional<VirtualToolIndex>(get_target_virtual_from_command());
  if (!virtual_tool_opt.has_value()) return;

  const auto virtual_tool = *virtual_tool_opt;

  if (parser.seenval('S')) {
    int16_t flow_percentage = parser.value_int();
    #if HAS_GCODE_COMPATIBILITY()
      if (gcode.compatibility.mk3_compatibility_mode) {
        flow_percentage = static_cast<int16_t>(flow_percentage / 0.95f);
      }
    #endif
    planner.flow_percentage[virtual_tool] = flow_percentage;
    planner.refresh_e_factor(virtual_tool);
  }
  else {
    SERIAL_ECHO_START();
    SERIAL_CHAR('E');
    SERIAL_CHAR('0' + virtual_tool.to_raw());
    SERIAL_ECHOPAIR(" Flow: ", planner.flow_percentage[virtual_tool]);
    SERIAL_CHAR('%');
    SERIAL_EOL();
  }
}

/** @}*/
