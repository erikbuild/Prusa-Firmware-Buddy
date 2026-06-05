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

#include "config_features.h"

#include <Marlin/src/gcode/gcode.h>
#include <Marlin/src/module/motion.h>
#include <common/gcode/gcode_parser.hpp>
#include <feature/filament_sensor/filament_sensors_handler.hpp>
#include <common/mapi/parking.hpp>

/** \addtogroup G-Codes
 * @{
 */

/**
 *### G27: Park the nozzle <a href="https://reprap.org/wiki/G-code#G27:_Park_toolhead">G27: Park toolhead</a>
 *
 *#### Usage
 *
 * G27 [ X | Y | Z | P ]
 *
 *#### Parameters
 *
 * - `X` - X park position
 * - `Y` - Y park position
 * - `Z` - Z park position
 * - `P` - Z action
 *   - `0` - (Default) Raise to at least Z before XY parking
 *   - `1` - Absolute move to Z before XY parking. This may move the nozzle down, so use with caution!
 *   - `2` - Relative move by Z before XY parking.
 * - `W` - Use pre-defined park position. Usable only if X, Y and Z are not present as they override pre-defined behaviour.
 *   - `0` - Park
 *   - `1` - Purge
 *   - `2` - Load
 */
void GcodeSuite::G27() {
    GCodeParser2 parser;
    if (!parser.parse_marlin_command()) {
        return;
    }

    mapi::ParkingPosition parking_position;

    if (auto where_to_park = parser.option<mapi::ParkPosition>('W', mapi::ParkPosition::_cnt)) {
        parking_position = mapi::get_parking_position(*where_to_park);

    } else {
        if (auto x = parser.option<float>('X')) {
            parking_position.x = *x;
        }

        if (auto y = parser.option<float>('Y')) {
            parking_position.y = *y;
        }

        if (auto z = parser.option<float>('Z')) {
            switch (parser.option<int>('P').value_or(0)) {

            case 0:
                parking_position.z = mapi::ParkingPosition::Minimum { .above_print = *z };
                break;

            case 1:
                parking_position.z = *z;
                break;

            case 2:
                parking_position.z = mapi::ParkingPosition::Relative { *z };
                break;
            }
        }

        // If no axis has been specified (comparing against a position with all axes unchanged)
        if (parking_position == mapi::ParkingPosition {}) {
            parking_position = mapi::get_parking_position(mapi::ParkPosition::park);
        }
    }

    mapi::home_if_needed_and_park(parking_position);
}

/** @}*/
