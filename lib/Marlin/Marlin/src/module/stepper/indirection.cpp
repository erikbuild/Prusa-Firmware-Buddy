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

/**
 * stepper/indirection.cpp
 *
 * Stepper motor driver indirection to allow some stepper functions to
 * be done via SPI/I2c instead of direct pin manipulation.
 *
 * Copyright (c) 2015 Dominik Wenger
 */

#include "indirection.h"
#include <bsod.h>

static void enable_or_disable_E(uint8_t index, bool value) {
    int pin_value = value ? E_ENABLE_ON : !E_ENABLE_ON;
    switch (index) {
#if (E_STEPPERS > 0 || HAS_TOOLCHANGER()) && HAS_E0_ENABLE
    case 0:
        E0_ENABLE_WRITE(pin_value);
        break;
#endif
#if (E_STEPPERS > 1 || HAS_TOOLCHANGER()) && HAS_E1_ENABLE
    case 1:
        E1_ENABLE_WRITE(pin_value);
        break;
#endif
#if (E_STEPPERS > 2 || HAS_TOOLCHANGER()) && HAS_E2_ENABLE
    case 2:
        E2_ENABLE_WRITE(pin_value);
        break;
#endif
#if (E_STEPPERS > 3 || HAS_TOOLCHANGER()) && HAS_E3_ENABLE
    case 3:
        E3_ENABLE_WRITE(pin_value);
        break;
#endif
#if (E_STEPPERS > 4 || HAS_TOOLCHANGER()) && HAS_E4_ENABLE
    case 4:
        E4_ENABLE_WRITE(pin_value);
        break;
#endif
#if (E_STEPPERS > 5 || HAS_TOOLCHANGER()) && HAS_E5_ENABLE
    case 5:
        E5_ENABLE_WRITE(pin_value);
        break;
#endif
    }
    static_assert(E_STEPPERS < 6);
}

void stepper_enable(AxisEnum axis, bool enabled) {
    switch (axis) {
#if ENABLED(XY_LINKED_ENABLE)
    case X_AXIS:
    case Y_AXIS:
        if (enabled) {
            enable_XY();
        } else {
            disable_XY();
        }
        break;
#else
    #if !SAME_DRIVER_ID(X_DRIVER_TYPE, NONE)
    case X_AXIS:
        if (enabled) {
            enable_X();
        } else {
            disable_X();
        }
        break;
    #endif
    #if !SAME_DRIVER_ID(Y_DRIVER_TYPE, NONE)
    case Y_AXIS:
        if (enabled) {
            enable_Y();
        } else {
            disable_Y();
        }
        break;
    #endif
#endif
#if !SAME_DRIVER_ID(Z_DRIVER_TYPE, NONE)
    case Z_AXIS:
        if (enabled) {
            enable_Z();
        } else {
            disable_Z();
        }
        break;
#endif
#if E_STEPPERS > 0
    case E0_AXIS:
        return enable_or_disable_E(0, enabled);
#endif
#if E_STEPPERS > 1
    case E1_AXIS:
        return enable_or_disable_E(1, enabled);
#endif
#if E_STEPPERS > 2
    case E2_AXIS:
        return enable_or_disable_E(2, enabled);
#endif
#if E_STEPPERS > 3
    case E3_AXIS:
        return enable_or_disable_E(3, enabled);
#endif
#if E_STEPPERS > 4
    case E4_AXIS:
        return enable_or_disable_E(4, enabled);
#endif
#if E_STEPPERS > 5
    case E5_AXIS:
        return enable_or_disable_E(5, enabled);
#endif
    default:
        bsod("invalid stepper axis");
    }
}

bool stepper_enabled(AxisEnum axis) {
    switch (axis) {
#if ENABLED(XY_LINKED_ENABLE)
    case X_AXIS:
    case Y_AXIS:
        return X_ENABLE_READ();
#else
    #if !SAME_DRIVER_ID(X_DRIVER_TYPE, NONE)
    case X_AXIS:
        return X_ENABLE_READ();
    #endif
    #if !SAME_DRIVER_ID(Y_DRIVER_TYPE, NONE)
    case Y_AXIS:
        return Y_ENABLE_READ();
    #endif
#endif
#if !SAME_DRIVER_ID(Z_DRIVER_TYPE, NONE)
    case Z_AXIS:
        return Z_ENABLE_READ();
#endif
#if E_STEPPERS > 0
    case E0_AXIS:
        return E0_ENABLE_READ();
#endif
#if E_STEPPERS > 1
    case E1_AXIS:
        return E1_ENABLE_READ();
#endif
#if E_STEPPERS > 2
    case E2_AXIS:
        return E2_ENABLE_READ();
#endif
#if E_STEPPERS > 3
    case E3_AXIS:
        return E3_ENABLE_READ();
#endif
#if E_STEPPERS > 4
    case E4_AXIS:
        return E4_ENABLE_READ();
#endif
#if E_STEPPERS > 5
    case E5_AXIS:
        return E5_ENABLE_READ();
#endif
    default:
        bsod("invalid stepper axis");
    }
}

void enable_E(uint8_t index) {
    enable_or_disable_E(index, true);
}

void disable_E(uint8_t index) {
    enable_or_disable_E(index, false);
}

// Yuri needs these functions
static_assert(requires { stepper_enabled; });
static_assert(requires { stepper_disable; });
