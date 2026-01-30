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

#include "../../inc/MarlinConfig.h"

#define OVERSAMPLENR 16
#define OV(N) int16_t((N) * (OVERSAMPLENR))

#define ANY_THERMISTOR_IS(n) (THERMISTOR_HEATER_0 == n || THERMISTOR_HEATER_1 == n || THERMISTOR_HEATER_2 == n || THERMISTOR_HEATER_3 == n || THERMISTOR_HEATER_4 == n || THERMISTOR_HEATER_5 == n || THERMISTORBED == n || TEMP_SENSOR_HEATBREAK == n  || TEMP_SENSOR_BOARD == n)

#if ANY_THERMISTOR_IS(1) // beta25 = 4092 K, R25 = 100 kOhm, Pull-up = 4.7 kOhm, "EPCOS"
  #include "thermistor_1.h"
#endif
#if ANY_THERMISTOR_IS(5) // beta25 = 4267 K, R25 = 100 kOhm, Pull-up = 4.7 kOhm, "ParCan, ATC 104GT-2"
  #include "thermistor_5.h"
#endif
#if ANY_THERMISTOR_IS(55) // beta25 = 4267 K, R25 = 100 kOhm, Pull-up = 1 kOhm, "ATC Semitec 104GT-2 (Used on ParCan)"
  #include "thermistor_55.h"
#endif
#if ANY_THERMISTOR_IS(2000) // 100k TDK NTC Chip Thermistor NTCG104LH104JT1 with 4k7 pullup
  #include "thermistor_2000.h"
#endif
#if ANY_THERMISTOR_IS(2004) // beta25 = 4390 100k Semitec NTC Thermistor 104 JT-025 with 4k7 pullup - ULTIMATE THINNESS, JT THERMISTOR
  #include "thermistor_2004.h"
#endif
#if ANY_THERMISTOR_IS(2005) // 100k TDK NTC Chip Thermistor NTCG104LH104JT1 with 4k7 pullup
  #include "thermistor_2005.h"
#endif
#if ANY_THERMISTOR_IS(2008) // XL prototype termistor, TODO: FIX
  #include "thermistor_2008.h"
#endif
#if ANY_THERMISTOR_IS(1010) // PT1000
  #include "thermistor_1010.h"
#endif

#define _TT_NAME(_N) temptable_ ## _N
#define TT_NAME(_N) _TT_NAME(_N)
