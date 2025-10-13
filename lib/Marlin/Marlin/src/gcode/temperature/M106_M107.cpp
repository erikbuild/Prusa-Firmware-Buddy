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

#if FAN_COUNT > 0

    #include "../gcode.h"
    #include "../../module/motion.h"
    #include "../../module/temperature.h"
    #include "fanctl.hpp"
    #include <device/board.h>
    #include <option/xbuddy_extension_variant_standard.h>

    #if XBUDDY_EXTENSION_VARIANT_STANDARD()
        #include <feature/xbuddy_extension/xbuddy_extension.hpp>
    #endif

    #if ENABLED(SINGLENOZZLE)
        #define _ALT_P active_extruder
        #define _CNT_P EXTRUDERS
    #elif ENABLED(PRUSA_TOOLCHANGER)
        #define _ALT_P 0
        #define _CNT_P FAN_COUNT
    #else
        #define _ALT_P _MIN(active_extruder, FAN_COUNT - 1)
        #define _CNT_P FAN_COUNT
    #endif

/**
 * @brief Set fans that are not controlled by Marlin
 * @param fan Fan index (0-based)
 * @param tool tool number (0-based)
 * @param speed Fan speed (0-255)
 * @param set_auto true to set auto control
 * @return false to let Marlin process this fan as well, true to eat this G-code
 */
static bool set_special_fan_speed(uint8_t fan, int8_t tool, uint8_t speed, bool set_auto) {
    #if HAS_TOOLCHANGER()
    if (fan == 1) { // Heatbreak fan
        if (tool >= 0 && tool <= buddy::puppies::DWARF_MAX_COUNT) {
            if (buddy::puppies::dwarfs[tool].is_enabled()) {
                if (set_auto) {
                    buddy::puppies::dwarfs[tool].set_fan_auto(1);
                } else {
                    buddy::puppies::dwarfs[tool].set_fan(1, speed);
                }
            }
        }
        return true; // Eat this G-code, heatbreak fan is not controlled by Marlin
    }
    #endif /* HAS_TOOLCHANGER() */

    #if XL_ENCLOSURE_SUPPORT()
    static_assert(FAN_COUNT < 3, "Fan index 3 is reserved for Enclosure fan and should not be set by thermalManager");
    if (fan == 3) {
        Fans::enclosure().set_pwm(speed);
        return true;
    }
    #endif

    #if XBUDDY_EXTENSION_VARIANT_STANDARD()
    using XBE = buddy::XBuddyExtension;
    static_assert(FAN_COUNT < 3, "Fan 3 is dedicated to extboard");

    switch (fan) {
    case 3:
        // Cooling fan 2 has shared PWM line
        buddy::xbuddy_extension().set_fan_target_pwm(XBE::Fan::cooling_fan_1, set_auto ? buddy::XBuddyExtension::FanPWMOrAuto(pwm_auto) : buddy::XBuddyExtension::FanPWM(speed));
        return true;
    case 4:
        buddy::xbuddy_extension().set_fan_target_pwm(XBE::Fan::filtration_fan, set_auto ? buddy::XBuddyExtension::FanPWMOrAuto(pwm_auto) : buddy::XBuddyExtension::FanPWM(speed));
        return true;
    default:
        break;
    }
    #endif

    return false;
}

/** \addtogroup G-Codes
 * @{
 */

/**
 *### M106: Set Fan Speed <a href="https://reprap.org/wiki/G-code#M106:_Fan_On">M106: Fan On</a>
 *
 *#### Usage
 *
 *   M106 [ S | P | T ]
 *
 *#### Parameters
 *
 * - `S` - Speed between 0-255
 * - `P` - Fan index, if more than one fan
 *     - `0` - Print fan
 *     - `1` - Heatbreak fan
 *     - `2` - ...
 *     - `3` - Cooling fan (if supported) or Enclosure fan (XL)
 *     - `4` - Filtration fan (if supported)
 * - `R` - Set the to auto control (if supported by the fan)
 * - `A` - ???
 * - `T` - Select which tool if the same fan is on multiple tools, active_extruder if not specified
 *Enclosure fan (index 3) don't support T parameter
 */
void GcodeSuite::M106() {
    const uint8_t p = parser.byteval('P', _ALT_P);
    const uint16_t speed = std::clamp<uint16_t>(parser.ushortval('S', 255), 0, 255);
    const bool auto_control = parser.seen('R');

    if (set_special_fan_speed(p, get_target_extruder_from_command(), speed, auto_control)) {
        return;
    }

    if (p < _CNT_P) {
        uint16_t d = parser.seen('A') ? thermalManager.fan_speed[0] : 255;
        uint16_t s = parser.ushortval('S', d);
        NOMORE(s, 255U);
    #if HAS_GCODE_COMPATIBILITY()
        if (gcode.compatibility.mk4_compatibility_mode) {
            s = (s * 7) / 10; // Converts speed to 70% of its values
        }
    #endif

        thermalManager.set_fan_speed(p, s);
    }
}

/**
 *### M107: Fan Off <a href="https://reprap.org/wiki/G-code#M107:_Fan_Off">M107: Fan Off</a>
 *
 *#### Usage
 *
 *    M107 [ P ]
 *
 *#### Parameters
 *
 * - `P` - Fan index
 *     - `0` - Print fan
 *     - `1` - Heatbreak fan
 *     - `2` - ...
 *     - `3` - Cooling fan (if supported) or Enclosure fan (XL)
 *     - `4` - Filtration fan (if supported)
 * - `T` - Select which tool if there are multiple fans, one on each tool
 */
void GcodeSuite::M107() {
    const uint8_t p = parser.byteval('P', _ALT_P);

    if (set_special_fan_speed(p, get_target_extruder_from_command(), 0, false)) {
        return;
    }

    thermalManager.set_fan_speed(p, 0);
}

/** @}*/

#endif // FAN_COUNT > 0
