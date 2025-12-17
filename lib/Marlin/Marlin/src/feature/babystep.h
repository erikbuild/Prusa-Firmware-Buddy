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
#pragma once

#include "../inc/MarlinConfigPre.h"

#if IS_CORE || ENABLED(BABYSTEP_XY)
  #define BS_TODO_AXIS(A) A
#else
  #define BS_TODO_AXIS(A) 0
#endif

class Babystep {
public:
  static volatile int16_t steps[BS_TODO_AXIS(Z_AXIS) + 1];
  static int16_t accum;                                     // Total babysteps in current edit

  static void add_steps(const AxisEnum axis, const int16_t distance);
  static void add_mm(const AxisEnum axis, const float &mm);
  static void task();
private:
  static void step_axis(const AxisEnum axis);
};

extern Babystep babystep;
