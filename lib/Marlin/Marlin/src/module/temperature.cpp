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
 * temperature.cpp - temperature control
 */

#include "temperature.h"
#include "endstops.h"
#include "safe_state.h"

#include "thermistor/thermistor_5.h" // for user-space recognition of XBuddy with older LoveBoard

#include "../Marlin.h"
#include "../lcd/ultralcd.h"
#include "../core/language.h"
#include "../HAL/shared/Delay.h"
#include "bsod.h"
#include "logging/log.hpp"
#include "metric.h"
#include "../../../../src/common/hwio.h"
#include <stdint.h>
#include <device/board.h>
#include "printers.h"
#include "MarlinPin.h"
#include <module/motion.h>
#include "../../../../src/common/adc.hpp"
#include "../marlin_stubs/skippable_gcode.hpp"
#include <module/temperature/marlin_temptable.hpp>
#include <module/temperature/steady_state_hotend.hpp>

#include <option/has_toolchanger.h>
#if HAS_TOOLCHANGER()
  #include <Marlin/src/module/prusa/toolchanger.h>
#endif

#include <option/board_is_master_board.h>
#if BOARD_IS_MASTER_BOARD()
#include <marlin_server.hpp>
#endif

#include <option/has_planner.h>
#if HAS_PLANNER()
  #include "planner.h"
#endif

#include <feature/print_status_message/print_status_message_guard.hpp>
#include <i18n.h>
#include <option/has_chamber_api.h>
#if HAS_CHAMBER_API()
#include "feature/chamber/chamber.hpp"
#endif

#if EITHER(BABYSTEPPING, PID_EXTRUSION_SCALING)
  #include "stepper.h"
#endif

#if ENABLED(BABYSTEPPING)
  #include "../feature/babystep.h"
  #if ENABLED(BABYSTEP_ALWAYS_AVAILABLE)
    #include "../gcode/gcode.h"
  #endif
#endif

#include "printcounter.h"

#if ENABLED(EMERGENCY_PARSER)
  #include "../feature/emergency_parser.h"
#endif

#if ENABLED(SINGLENOZZLE)
  #include "tool_change.h"
#endif

#include <option/board_is_master_board.h>
#if BOARD_IS_MASTER_BOARD()
  #include <feature/safety_timer/safety_timer.hpp>
#endif

#include <option/has_ac_controller.h>
#include <option/has_dwarf.h>
#include <option/has_local_bed.h>
#include <option/has_remote_bed.h>
#include <option/has_modular_bed.h>
#include <utils/serial_logging_disabler.hpp>
#include <raii/scope_guard.hpp>

#if ENABLED(MODEL_BASED_HOTEND_REGULATOR)
  #include <module/temperature/hotend_regulator/model_based_hotend_regulator.hpp>
  using HotendRegulator = ModelBasedHotendRegulator;
#else
  #include <module/temperature/hotend_regulator/standard_hotend_regulator.hpp>
  using HotendRegulator = StandardHotendRegulator;
#endif

HotendRegulator hotend_regulators[HOTENDS];

#if HAS_AC_CONTROLLER()
    #include <puppies/ac_controller.hpp>
#endif

LOG_COMPONENT_REF(MarlinServer);

#if HOTEND_USES_THERMISTOR
    static constexpr MarlinTempTable heater_ttbl_map[HOTENDS] = ARRAY_BY_HOTENDS(TT_NAME(THERMISTOR_HEATER_0), TT_NAME(THERMISTOR_HEATER_1), TT_NAME(THERMISTOR_HEATER_2), TT_NAME(THERMISTOR_HEATER_3), TT_NAME(THERMISTOR_HEATER_4), TT_NAME(THERMISTOR_HEATER_5));
    static_assert(HOTENDS <= 6);
#endif

#if ENABLED(HW_PWM_HEATERS)
  static constexpr uint8_t soft_pwm_bit_shift = 0;
#else
  static constexpr uint8_t soft_pwm_bit_shift = 1;
#endif

// Rough estimate of room temperature
static constexpr float room_temperature = 25.0f;

Temperature thermalManager;

/**
 * Macros to include the heater id in temp errors. The compiler's dead-code
 * elimination should (hopefully) optimize out the unused strings.
 */

#if HAS_HEATED_BED
  #define _BED_PSTR(h) (h) == H_BED ? GET_TEXT(MSG_BED) :
#else
  #define _BED_PSTR(h)
#endif
#if HAS_TEMP_HEATBREAK_CONTROL
  #define _HEATBREAK_PSTR(h,N) ((HOTENDS) > N && (H_HEATBREAK_FIRST + h) == N) ? GET_TEXT(MSG_HEATBREAK) :
#else
  #define _HEATBREAK_PSTR(h,N)
#endif
#define _E_PSTR(h,N) ((HOTENDS) > N && (h) == N) ? PSTR(LCD_STR_E##N) :
#define HEATER_PSTR(h) _BED_PSTR(h) _E_PSTR(h,1) _E_PSTR(h,2) _E_PSTR(h,3) _E_PSTR(h,4) _E_PSTR(h,5) _HEATBREAK_PSTR(h,0) _HEATBREAK_PSTR(h,1) _HEATBREAK_PSTR(h,2) _HEATBREAK_PSTR(h,3) _HEATBREAK_PSTR(h,4) _HEATBREAK_PSTR(h,5) PSTR(LCD_STR_E0)

#define MIN_BED_POWER 0
#define MAX_BED_POWER 255
#if HAS_LOCAL_BED()
  #define WRITE_HEATER_BED(v) analogWrite_HEATER_BED((v) ? MAX_BED_POWER : MIN_BED_POWER)
#else
  #define WRITE_HEATER_BED(v)
#endif

// public:

StrongIndexArray<hotend_info_t, HOTENDS, PhysicalToolIndex, PhysicalToolIndex::to_raw_static, strong_index_array::AllowWeakIndexing::yes> Temperature::temp_hotend;
StrongIndexArray<uint32_t, HOTENDS, PhysicalToolIndex, PhysicalToolIndex::to_raw_static, strong_index_array::AllowWeakIndexing::yes> Temperature::temp_hotend_residency_start_ms;

#if FAN_COUNT > 0

  uint8_t Temperature::fan_speed[FAN_COUNT] = {};
  uint8_t Temperature::applied_fan_speed[FAN_COUNT] = {};

  uint16_t Temperature::get_fan_speed(const uint8_t target) {
    return target < FAN_COUNT ? fan_speed[target] : 0;
  }
  /**
   * Set the print fan speed for a target extruder
   * @note You need to call applyScaledFanSpeed() either from planner or elsewhere to actually use the configured fan speed.
   * Set the print fan speed for a target FAN
   * !!! NOT EXTRUDER !!! THERMAL MANAGER DOES NOT WORK WITH NON-ACTIVE EXTRUDER FANS
   * See BFW-6365
   */
  void Temperature::set_fan_speed(uint8_t target, uint16_t speed) {

    NOMORE(speed, 255U);

    // @@TODO hotfix for driving of the front fan (index 1) even with the MMU code
    // It is yet unknown if there are any side effects of commenting out this piece of code.
    // The singlenozzle_fan_speed is only used in tool_change and only in a part, which is not compiled
    // in our configuration.
//    #if ENABLED(SINGLENOZZLE)
//      if (target != active_extruder) {
//        if (target < EXTRUDERS) singlenozzle_fan_speed[target] = speed;
//        return;
//      }
//      target = 0; // Always use fan index 0 with SINGLENOZZLE
//    #endif

    if (target >= FAN_COUNT) return;

    fan_speed[target] = speed;
  }

#endif // FAN_COUNT > 0

#if WATCH_HOTENDS
  heater_watch_t Temperature::watch_hotend[HOTENDS]; // = { { 0 } }
#endif
#if HEATER_IDLE_HANDLER
  heater_idle_t Temperature::hotend_idle[HOTENDS]; // = { { 0 } }
#endif

#if HAS_TEMP_HEATBREAK
  StrongIndexArray<heatbreak_info_t, HOTENDS, PhysicalToolIndex, PhysicalToolIndex::to_raw_static, strong_index_array::AllowWeakIndexing::yes> Temperature::temp_heatbreak;

  static MarlinTemptableRawMinMax minmaxtemp_raw_HEATBREAK;

  #if WATCH_HEATBREAK
    heater_watch_t Temperature::watch_heatbreak[HOTENDS] = {0};
  #endif
  millis_t Temperature::next_heatbreak_check_ms;

#endif

#if HAS_TEMP_BOARD
  board_info_t Temperature::temp_board; // = { 0 }

#endif

#if HAS_HEATED_BED
  bed_info_t Temperature::temp_bed; // = { 0 }
  float Temperature::bed_frame_est_celsius = TempInfo::celsius_uninitialized;
  uint32_t Temperature::bed_frame_millis = 0;

  // Init min and max temp with extreme values to prevent false errors during startup
  static MarlinTemptableRawMinMax minmaxtemp_raw_BED;

  #if WATCH_BED
    heater_watch_t Temperature::watch_bed; // = { 0 }
  #endif
  #if DISABLED(PIDTEMPBED)
    millis_t Temperature::next_bed_check_ms;
  #endif
  #if HEATER_IDLE_HANDLER
    heater_idle_t Temperature::bed_idle; // = { 0 }
  #endif
#endif // HAS_HEATED_BED

// Initialized by settings.load()
#if ENABLED(PIDTEMP)
  //hotend_pid_t Temperature::pid[HOTENDS];
#endif

#if PRINTER_IS_PRUSA_iX()
  TempInfo Temperature::temp_psu;
  TempInfo Temperature::temp_ambient;
#endif

#if ENABLED(PREVENT_COLD_EXTRUSION)
  bool Temperature::allow_cold_extrude = false;
  int16_t Temperature::extrude_min_temp = EXTRUDE_MINTEMP;
#endif

// private:

#if EARLY_WATCHDOG
  bool Temperature::inited = false;
#endif

volatile bool Temperature::temp_meas_ready = false;

#if ENABLED(PID_EXTRUSION_SCALING)
  uint32_t Temperature::last_e_position;
  bool Temperature::extrusion_scaling_enabled = true;
#endif

struct temp_range_t {
  int16_t mintemp, maxtemp;
  MarlinTemptableRawMinMax raw;

  temp_range_t(MarlinTempTable temptable, int16_t mintemp, int16_t maxtemp)
    : mintemp(mintemp)
    , maxtemp(maxtemp)
    , raw(MarlinTemptableRawMinMax::compute(temptable, mintemp, maxtemp))
    {}
};

#define TEMP_RANGE_INIT(i) temp_range_t(TT_NAME(THERMISTOR_HEATER_ ## i), HEATER_ ## i ## _MINTEMP, HEATER_ ## i ## _MAXTEMP)
                        
static StrongIndexArray<temp_range_t, HOTENDS, PhysicalToolIndex, PhysicalToolIndex::to_raw_static, strong_index_array::AllowWeakIndexing::yes> temp_range {
  #if HOTENDS > 0
    TEMP_RANGE_INIT(0),
  #endif
  #if HOTENDS > 1
    TEMP_RANGE_INIT(1),
  #endif
  #if HOTENDS > 2
    TEMP_RANGE_INIT(2),
  #endif
  #if HOTENDS > 3
    TEMP_RANGE_INIT(3),
  #endif
  #if HOTENDS > 4
    TEMP_RANGE_INIT(4),
  #endif
  #if HOTENDS > 5
    TEMP_RANGE_INIT(5),
  #endif
};
static_assert(HOTENDS <= 6);

#undef TEMP_RANGE_INIT

#if HAS_AUTO_FAN
  millis_t Temperature::next_auto_fan_check_ms = 0;
#endif

// public:

/**
 * Class and Instance Methods
 */

int16_t Temperature::getHeaterPower(const heater_ind_t heater_id) {
  #if HAS_HEATED_BED
    if (heater_id == H_BED) {
      #if HAS_REMOTE_BED()
        return 0;
      #else
        return temp_bed.soft_pwm_amount;
      #endif
    }
  #endif
  if (heater_id >= H_NOZZLE_FIRST && heater_id <= H_NOZZLE_LAST) {
    const uint8_t tool_id = heater_id - (uint8_t)H_NOZZLE_FIRST;
    #if HAS_TOOLCHANGER()
      return prusa_toolchanger.getTool(tool_id).get_heater_pwm();
    #else
      return temp_hotend[tool_id].soft_pwm_amount;
    #endif
  }
  #if HAS_TEMP_HEATBREAK
    if (heater_id >= H_HEATBREAK_FIRST && heater_id <= H_HEATBREAK_LAST) {
      const uint8_t tool_id = heater_id - (uint8_t)H_HEATBREAK_FIRST;
      #if HAS_TOOLCHANGER()
        return prusa_toolchanger.getTool(tool_id).get_heatbreak_fan_pwr();
      #else
        return temp_heatbreak[tool_id].soft_pwm_amount;
      #endif
    }
  #endif //HAS_TEMP_HEATBREAK

  return 0;
}

#define _EFANOVERLAP(A,B) _FANOVERLAP(E##A,B)

#if HAS_AUTO_FAN

  void Temperature::checkExtruderAutoFans() {
    #define _EFAN(A,B) _EFANOVERLAP(A,B) ? B :
    static const uint8_t fanBit[] PROGMEM = {
      0
      #if HOTENDS > 1
        , _EFAN(1,0) 1
      #endif
      #if HOTENDS > 2
        , _EFAN(2,0) _EFAN(2,1) 2
      #endif
      #if HOTENDS > 3
        , _EFAN(3,0) _EFAN(3,1) _EFAN(3,2) 3
      #endif
      #if HOTENDS > 4
        , _EFAN(4,0) _EFAN(4,1) _EFAN(4,2) _EFAN(4,3) 4
      #endif
      #if HOTENDS > 5
        , _EFAN(5,0) _EFAN(5,1) _EFAN(5,2) _EFAN(5,3) _EFAN(5,4) 5
      #endif
    };
    uint8_t fanState = 0;

    for (int8_t e = 0; e < HOTENDS; e++)
      if (temp_hotend[e].celsius >= EXTRUDER_AUTO_FAN_TEMPERATURE)
        SBI(fanState, pgm_read_byte(&fanBit[e]));

    #define _UPDATE_AUTO_FAN(P,D,A) do{                  \
      if (PWM_PIN(P##_AUTO_FAN_PIN) && A < 255)          \
        analogWrite(pin_t(P##_AUTO_FAN_PIN), D ? A : 0); \
      else                                               \
        WRITE(P##_AUTO_FAN_PIN, D);                      \
    }while(0)

    uint8_t fanDone = 0;
    for (uint8_t f = 0; f < COUNT(fanBit); f++) {
      const uint8_t realFan = pgm_read_byte(&fanBit[f]);
      if (TEST(fanDone, realFan)) continue;
      const bool fan_on = TEST(fanState, realFan);
      switch (f) {
        #if HAS_AUTO_FAN_0
          case 0: _UPDATE_AUTO_FAN(E0, fan_on, EXTRUDER_AUTO_FAN_SPEED); break;
        #endif
        #if HAS_AUTO_FAN_1
          case 1: _UPDATE_AUTO_FAN(E1, fan_on, EXTRUDER_AUTO_FAN_SPEED); break;
        #endif
        #if HAS_AUTO_FAN_2
          case 2: _UPDATE_AUTO_FAN(E2, fan_on, EXTRUDER_AUTO_FAN_SPEED); break;
        #endif
        #if HAS_AUTO_FAN_3
          case 3: _UPDATE_AUTO_FAN(E3, fan_on, EXTRUDER_AUTO_FAN_SPEED); break;
        #endif
        #if HAS_AUTO_FAN_4
          case 4: _UPDATE_AUTO_FAN(E4, fan_on, EXTRUDER_AUTO_FAN_SPEED); break;
        #endif
        #if HAS_AUTO_FAN_5
          case 5: _UPDATE_AUTO_FAN(E5, fan_on, EXTRUDER_AUTO_FAN_SPEED); break;
        #endif
      }
      SBI(fanDone, realFan);
    }
  }

#endif // HAS_AUTO_FAN

//
// Temperature Error Handlers
//

inline void loud_kill(PGM_P const lcd_msg, const heater_ind_t heater) {
  Running = false;
  kill(lcd_msg, HEATER_PSTR(heater));
}

void Temperature::_temp_error(const heater_ind_t heater, PGM_P const serial_msg, PGM_P const lcd_msg) {
  if (IsRunning()) {
    SERIAL_ERROR_START();
    serialprintPGM(serial_msg);
    SERIAL_ECHOPGM(MSG_STOPPED_HEATER);
    if (heater >= 0) SERIAL_ECHO((int)heater);
    else SERIAL_ECHOPGM(MSG_HEATER_BED);
    SERIAL_EOL();
  }

  // Disable only the local heaters. We are in ISR, so we can't afford any kind
  // of interprocessor communication and it would not finish anyway before we
  // kill it.
  disable_local_heaters(); // always disable (even for bogus temp)

  loud_kill(lcd_msg, heater);
}

void Temperature::max_temp_error(const heater_ind_t heater) {
  #if HAS_HEATED_BED
    if (H_BED == heater) {
      _temp_error(heater, PSTR(MSG_T_MAXTEMP), GET_TEXT(MSG_ERR_MAXTEMP_BED));
      return;
    }
  #endif
  #if HAS_TEMP_HEATBREAK
    //we have multiple heartbreak thermistors and they have always the highest ID
    if(heater >= H_HEATBREAK_FIRST){
        _temp_error(heater, PSTR(MSG_T_MAXTEMP), GET_TEXT(MSG_ERR_MAXTEMP_HEATBREAK));
    }
  #endif
  _temp_error(heater, PSTR(MSG_T_MAXTEMP), GET_TEXT(MSG_ERR_MAXTEMP));
}

void Temperature::min_temp_error(const heater_ind_t heater) {
  #if HAS_HEATED_BED
    if (H_BED == heater) {
      _temp_error(heater, PSTR(MSG_T_MINTEMP), GET_TEXT(MSG_ERR_MINTEMP_BED));
      return;
    }
  #endif
  #if HAS_TEMP_HEATBREAK
    //we have multiple heartbreak thermistors and they have always the highest ID
    if(heater >= H_HEATBREAK_FIRST){
        _temp_error(heater, PSTR(MSG_T_MINTEMP), GET_TEXT(MSG_ERR_MINTEMP_HEATBREAK));
    }
  #endif
  _temp_error(heater, PSTR(MSG_T_MINTEMP), GET_TEXT(MSG_ERR_MINTEMP));
}

#if ENABLED(PIDTEMPBED)

  float Temperature::get_pid_output_bed() {

    #if DISABLED(PID_OPENLOOP)

      static PID_t work_pid{0};
      static float temp_iState = 0, temp_dState = 0;
      static bool pid_reset = true;
      float pid_output = 0;
      const float pid_error = temp_bed.target - temp_bed.celsius;

      if (!temp_bed.target || pid_error < -(PID_FUNCTIONAL_RANGE)) {
        pid_output = 0;
        pid_reset = true;
      }
      else if (pid_error > PID_FUNCTIONAL_RANGE) {
        pid_output = MAX_BED_POWER;
        pid_reset = true;
      }
      else {
        if (pid_reset) {
          temp_iState = 0.0;
          work_pid.Kd = 0.0;
          pid_reset = false;
        }

        work_pid.Kp = temp_bed.pid.Kp * pid_error;
        work_pid.Kd = work_pid.Kd + PID_K2 * (temp_bed.pid.Kd * (pid_error - temp_dState) - work_pid.Kd);

        pid_output = work_pid.Kp + work_pid.Kd + float(MIN_BED_POWER);

        //Sum error only if it has effect on output value
        if (!((((pid_output + work_pid.Ki) < 0) && (pid_error < 0))
           || (((pid_output + work_pid.Ki) > MAX_BED_POWER) && (pid_error > 0 )))) {
          temp_iState += pid_error;
        }
        work_pid.Ki = temp_bed.pid.Ki * temp_iState;

        pid_output += work_pid.Ki;
        temp_dState = pid_error;

        pid_output = constrain(pid_output, 0, MAX_BED_POWER); //TODO shouldn't be low limit MIN_BED_POWER?
      }

    #else // PID_OPENLOOP

      const float pid_output = constrain(temp_bed.target, 0, MAX_BED_POWER);

    #endif // PID_OPENLOOP

    #if ENABLED(PID_BED_DEBUG)
    {
      SERIAL_ECHO_START();
      SERIAL_ECHOLNPAIR(
        " PID_BED_DEBUG : Input ", temp_bed.celsius, " Output ", pid_output,
        #if DISABLED(PID_OPENLOOP)
          MSG_PID_DEBUG_PTERM, work_pid.Kp,
          MSG_PID_DEBUG_ITERM, work_pid.Ki,
          MSG_PID_DEBUG_DTERM, work_pid.Kd,
        #endif
      );
    }
    #endif

    return pid_output;
  }

#endif // PIDTEMPBED

#if ENABLED(PIDTEMPHEATBREAK)

#include <module/temperature/heatbreak_regulator.hpp>
static HeatbreakRegulator heatbreak_regulator[HOTENDS];

#endif // PIDTEMPBED

void Temperature::update_temp_residency_hotend(uint8_t hotend) {
  const float temp_diff = ABS(temp_hotend[hotend].target - temp_hotend[hotend].celsius);

  if (!temp_hotend_residency_start_ms[hotend] && temp_diff < TEMP_WINDOW) {
    // Start the TEMP_RESIDENCY_TIME timer when we reach target temp for the first time.
    temp_hotend_residency_start_ms[hotend] = millis();
  } else if (temp_diff > TEMP_HYSTERESIS) {
    // Restart the timer whenever the temperature falls outside the hysteresis.
    temp_hotend_residency_start_ms[hotend] = 0;
  }
}

/**
 * Manage heating activities for extruder hot-ends and a heated bed
 *  - Acquire updated temperature readings
 *    - Also resets the watchdog timer
 *  - Invoke thermal runaway protection
 *  - Manage extruder auto-fan
 *  - Apply filament width to the extrusion rate (may move)
 *  - Update the heated bed PID output value
 *  - Kickstart fans
 */
void Temperature::manage_heater() {

  #if EARLY_WATCHDOG
    // If thermal manager is still not running, make sure to at least reset the watchdog!
    if (!inited) return watchdog_refresh();
  #endif

  #if ENABLED(EMERGENCY_PARSER)
    if (emergency_parser.killed_by_M112) kill();
  #endif

  if (!temp_meas_ready) return;

  updateTemperaturesFromRawValues(); // also resets the watchdog

  millis_t ms = millis();

  #if ENABLED(PID_EXTRUSION_SCALING)
    uint32_t e_position = stepper.position(E_AXIS);
    static constexpr float sample_frequency = TEMP_TIMER_FREQUENCY / MIN_ADC_ISR_LOOPS / OVERSAMPLENR;
    constexpr float distance_to_volume = std::numbers::pi_v<float> * std::pow(DEFAULT_NOMINAL_FILAMENT_DIA / 2, 2.f);
    constexpr float distance_to_volume_per_second = distance_to_volume * sample_frequency;
    const float e_volume_delta = (e_position - last_e_position) * planner.mm_per_step[E_AXIS] * distance_to_volume_per_second;
    last_e_position = e_position;
  #endif

  const auto current_tool = PhysicalToolIndex::currently_selected();

  for (int8_t e = 0; e < HOTENDS; e++) {
    const auto tool = PhysicalToolIndex::from_raw_notool(e);

    #if ENABLED(THERMAL_PROTECTION_HOTENDS)
      if (degHotend(e) > temp_range[e].maxtemp)
        _temp_error((heater_ind_t)e, PSTR(MSG_T_THERMAL_RUNAWAY), GET_TEXT(MSG_THERMAL_RUNAWAY));
    #endif

    #if HEATER_IDLE_HANDLER
      hotend_idle[e].update(ms);
    #endif
    
    update_temp_residency_hotend(e);

    #if ENABLED(THERMAL_PROTECTION_HOTENDS)
      // Check for thermal runaway
      thermal_runaway_hotends[e].step(temp_hotend[e].celsius, temp_hotend[e].target, (heater_ind_t)e, THERMAL_PROTECTION_PERIOD, THERMAL_PROTECTION_HYSTERESIS, thermalManager.hotend_idle[e].timed_out);
    #endif

    HotendRegulatorResult regulation_result {
      .pid_output = 0,
      .feed_forward = 0,
    };

    if(temp_hotend[e].celsius > temp_range[e].mintemp && temp_hotend[e].celsius < temp_range[e].maxtemp) {
      regulation_result = hotend_regulators[e].get_pid_output_hotend(HotendRegulatorArgs{
        .hotend_index = (uint8_t)e,
        .fan_speed = fan_speed[0], // FIXME: Bit of a cockup if we have multiple hotends.
        .current_temp = temp_hotend[e].celsius,
        .target_temp = temp_hotend[e].target,
      #if HEATER_IDLE_HANDLER
        .reset_pid = hotend_idle[e].timed_out,
      #endif
      #if ENABLED(PID_EXTRUSION_SCALING)
        .e_volume_delta = (extrusion_scaling_enabled && tool == current_tool) ? e_volume_delta : 0,
      #endif
      });
    }
    
    temp_hotend[e].soft_pwm_amount = static_cast<int>(regulation_result.pid_output) >> soft_pwm_bit_shift;
    #if ENABLED(MODEL_DETECT_STUCK_THERMISTOR)
        thermal_model_protection(regulation_result.pid_output, regulation_result.feed_forward, e);
    #endif

    #if WATCH_HOTENDS
      if (hotend_idle[e].timed_out)
        start_watching_hotend(e);
      else
        // Make sure temperature is increasing
        if (watch_hotend[e].next_ms && ELAPSED(ms, watch_hotend[e].next_ms)) { // Time to check this extruder?
          if (degHotend(e) < watch_hotend[e].target)                           // Failed to increase enough?
            _temp_error((heater_ind_t)e, PSTR(MSG_T_HEATING_FAILED), GET_TEXT(MSG_HEATING_FAILED_LCD));
          else                                                                 // Start again if the target is still far off
            start_watching_hotend(e);
        }
    #endif

  } // HOTEND_LOOP

  #if HAS_AUTO_FAN
    if (ELAPSED(ms, next_auto_fan_check_ms)) { // only need to check fan state very infrequently
      checkExtruderAutoFans();
      next_auto_fan_check_ms = ms + 2500UL;
    }
  #endif

  #if HAS_HEATED_BED

    #if ENABLED(THERMAL_PROTECTION_BED)
      if (degBed() > BED_MAXTEMP)
        _temp_error(H_BED, PSTR(MSG_T_THERMAL_RUNAWAY), GET_TEXT(MSG_THERMAL_RUNAWAY_BED));
    #endif

    #if WATCH_BED
      // Make sure temperature is increasing
      if (watch_bed.elapsed(ms)) {        // Time to check the bed?
        if (degBed() < watch_bed.target)                                // Failed to increase enough?
          _temp_error(H_BED, PSTR(MSG_T_HEATING_FAILED), GET_TEXT(MSG_HEATING_FAILED_LCD_BED));
        else                                                            // Start again if the target is still far off
          start_watching_bed();
      }
    #endif // WATCH_BED

    do {

      #if DISABLED(PIDTEMPBED)
        if (PENDING(ms, next_bed_check_ms)
        ) break;
        next_bed_check_ms = ms + 5000; // ms between checks in bang-bang control
      #endif

      #if HEATER_IDLE_HANDLER
        bed_idle.update(ms);
      #endif

      #if HAS_THERMALLY_PROTECTED_BED
        thermal_runaway_bed.step(temp_bed.celsius, temp_bed.target, H_BED, THERMAL_PROTECTION_BED_PERIOD, THERMAL_PROTECTION_BED_HYSTERESIS, thermalManager.bed_idle.timed_out);
      #endif

      #if HEATER_IDLE_HANDLER
        if (bed_idle.timed_out) {
          temp_bed.soft_pwm_amount = 0;
          #if DISABLED(PIDTEMPBED)
            WRITE_HEATER_BED(LOW);
          #endif
        }
        else
      #endif
      {
        #if ENABLED(PIDTEMPBED)
          temp_bed.soft_pwm_amount = WITHIN(temp_bed.celsius, BED_MINTEMP, BED_MAXTEMP) ? (int)get_pid_output_bed() >> soft_pwm_bit_shift : 0;
        #else
          // Check if temperature is within the correct band
          if (WITHIN(temp_bed.celsius, BED_MINTEMP, BED_MAXTEMP)) {
              temp_bed.soft_pwm_amount = temp_bed.celsius < temp_bed.target ? MAX_BED_POWER >> 1 : 0;
          }
          else {
            temp_bed.soft_pwm_amount = 0;
            WRITE_HEATER_BED(LOW);
          }
        #endif
      }

    } while (false);

  #endif

  #if HAS_TEMP_HEATBREAK

    #ifndef HEATBREAK_CHECK_INTERVAL
      #define HEATBREAK_CHECK_INTERVAL 1000UL
    #endif

    #if WATCH_HEATBREAK
    static_assert(HOTENDS == 1, "Multiple hotends not implemented" );
    if (watch_heatbreak[0].elapsed(ms)) { // Time to check the heatbreak?
        if (degHeatbreak(0) > watch_heatbreak[0].target) { // Failed to cool down enough
          fatal_error(ErrCode::ERR_TEMPERATURE_HEATBREAK_COOLING_TOO_SLOW); // Red screen
        } else {
          start_watching_heatbreak(0); // Start again if the target still was not reached
        }
      }
    #endif

    if (ELAPSED(ms, next_heatbreak_check_ms)) {
      next_heatbreak_check_ms = ms + HEATBREAK_CHECK_INTERVAL;

      #if HAS_TOOLCHANGER()
          // fan is regulted on dwarf - just update marlin's PWM value
          set_fan_speed(HEATBREAK_FAN_ID, prusa_toolchanger.getActiveToolOrFirst().get_heatbreak_fan_pwr());
      #else
        #if HOTENDS > 1
          #error not supported
        #endif
        // iX has a non-constant maxtemp for the heatbreak, so we need to explicitly set it
        #if PRINTER_IS_PRUSA_iX()
        int16_t heatbreak_maxtemp = degTargetHeatbreak(active_extruder) + HEATBREAK_MAXTEMP_OFFSET;
        #else
        int16_t heatbreak_maxtemp = HEATBREAK_MAXTEMP;
        #endif
        if (WITHIN(temp_heatbreak[0].celsius, HEATBREAK_MINTEMP, heatbreak_maxtemp)) {
          #if ENABLED(HEATBREAK_LIMIT_SWITCHING)
            if (temp_heatbreak[0].celsius >= temp_heatbreak[0].target + TEMP_HEATBREAK_HYSTERESIS)
              temp_heatbreak[0].soft_pwm_amount = 0;
            else if (temp_heatbreak[0].celsius <= temp_heatbreak[0].target - (TEMP_HEATBREAK_HYSTERESIS))
              temp_heatbreak[0].soft_pwm_amount = MAX_HEATBREAK_POWER >> 1;
          #elif ENABLED(PIDTEMPHEATBREAK)
            temp_heatbreak[0].soft_pwm_amount = (int)heatbreak_regulator[0].step();
            set_fan_speed(HEATBREAK_FAN_ID, temp_heatbreak[0].soft_pwm_amount);
          #endif
        } else {
          #if WATCH_HEATBREAK
          if(watch_heatbreak[0].next_ms == 0) { // if we are not watching heatbreak (not in process of cooling down)
            fatal_error(ErrCode::ERR_TEMPERATURE_HEATBREAK_MAXTEMP_ERR); // Red screen
          }
          #endif
          temp_heatbreak[0].soft_pwm_amount = 255;
          set_fan_speed(HEATBREAK_FAN_ID, temp_heatbreak[0].soft_pwm_amount);
        }
      #endif
    }
  #endif // HAS_TEMP_HEATBREAK

  UNUSED(ms);
  #if ENABLED(HW_PWM_HEATERS)
    #if HOTENDS == 1
      analogWrite(HEATER_0_PIN, temp_hotend[0].soft_pwm_amount);
    #elif HOTENDS
      #error "This is made for one hotend!"
    #endif /* HOTENDS */
    #if HAS_LOCAL_BED()
      analogWrite_HEATER_BED(temp_bed.soft_pwm_amount);
    #endif
  #endif

  #if HAS_FAN0
    analogWrite(FAN0_PIN, applied_fan_speed[0]);
  #endif
  #if HAS_FAN1
    analogWrite(FAN1_PIN, applied_fan_speed[1]);
  #endif
  #if HAS_FAN2
    analogWrite(FAN2_PIN, applied_fan_speed[2]);
  #endif
}

static bool temperatures_ready_state = false;

bool Temperature::temperatures_ready() {
  return temperatures_ready_state;
}

bool Temperature::are_all_temperatures_reached() {
  #if HAS_TEMP_HOTEND
    if(!are_hotend_temperatures_reached()) {
      return false;
    }
  #endif

  #if HAS_HEATED_BED
    if (!is_bed_temperature_reached()) {
      return false;
    }
  #endif

  return true;
}

#if HAS_TEMP_HEATBREAK && HAS_TEMP_HEATBREAK_CONTROL
void Temperature::suspend_heatbreak_fan(millis_t ms) {
  // TODO: why do have next_heatbreak_check_ms instead of using the nicer watch_heatbreak?
  next_heatbreak_check_ms = millis() + ms;

  for (auto tool : PhysicalToolIndex::all()) {
    temp_heatbreak[tool].soft_pwm_amount = 0;
  }
  WRITE_HEATER_HEATBREAK(LOW);
}
#endif

// Derived from RepRap FiveD extruder::getTemperature()
// For hot end temperature measurement.
float Temperature::analog_to_celsius_hotend(const int raw, const uint8_t e) {
    if (e >= HOTENDS)
    {
      SERIAL_ERROR_START();
      SERIAL_ECHO((int)e);
      SERIAL_ECHOLNPGM(MSG_INVALID_EXTRUDER_NUM);
      kill(PSTR(MSG_INVALID_EXTRUDER_NUM));
      return 0.0;
    }

  #if HAS_TOOLCHANGER()
    return prusa_toolchanger.getTool(e).get_hotend_temp();
  #endif

  #if HOTEND_USES_THERMISTOR
    // Thermistor with conversion table?
    return marlin_temptable_lookup(heater_ttbl_map[e], raw);
  #endif

  return 0;
}
#if HAS_HEATED_BED

#if PRINTER_IS_PRUSA_MK3_5() || PRINTER_IS_PRUSA_MK4() || PRINTER_IS_PRUSA_COREONE()
constexpr float compensate_bed_temperature(float celsius) {
  float _offset = 10;
  float _offset_center = 50;
  float _offset_start = 40;
  float _first_koef = (_offset / 2) / (_offset_center - _offset_start);
  float _second_koef = (_offset / 2) / (100 - _offset_center);

  if (celsius >= _offset_start && celsius <= _offset_center) {
      celsius = celsius + (_first_koef * (celsius - _offset_start));
  } else if (celsius > _offset_center && celsius <= 100) {
      celsius = celsius + (_first_koef * (_offset_center - _offset_start)) + ( _second_koef * ( celsius - ( 100 - _offset_center ) )) ;
  } else if (celsius > 100) {
      celsius = celsius + _offset;
  }
  return celsius;
}
#elif PRINTER_IS_PRUSA_MINI() || PRINTER_IS_PRUSA_XL() || PRINTER_IS_PRUSA_iX() || PRINTER_IS_PRUSA_XL_DEV_KIT() || PRINTER_IS_PRUSA_COREONEL()
constexpr float compensate_bed_temperature(float celsius) {
  return celsius;
}
#else
#error
#endif

  float scan_thermistor_table_bed(const int raw){
      return marlin_temptable_lookup(TT_NAME(THERMISTORBED), raw);
  }
  // Derived from RepRap FiveD extruder::getTemperature()
  // For bed temperature measurement.
  float Temperature::analog_to_celsius_bed(const int raw) {
    #if ENABLED(HEATER_BED_USES_THERMISTOR)
      float celsius = scan_thermistor_table_bed(raw);
      celsius = compensate_bed_temperature(celsius);
      return celsius;
    #elif HAS_MODULARBED()
      return raw
    #else
      return 0;
    #endif
  }
#endif // HAS_HEATED_BED

#if HAS_TEMP_HEATBREAK
  MarlinTempTable heatbreak_temptable() {
    #if (BOARD_IS_XBUDDY())
        if (buddy::hw::Configuration::Instance().needs_heatbreak_thermistor_table_5()) {
            return TT_NAME(5);
        }
    #endif

    return TT_NAME(TEMP_SENSOR_HEATBREAK);
  }

  // Derived from RepRap FiveD extruder::getTemperature()
  // For heatbreak temperature measurement.
  float Temperature::analog_to_celsius_heatbreak(const int raw) {
    #if ENABLED(HEATBREAK_USES_THERMISTOR)
      return marlin_temptable_lookup(heatbreak_temptable(), raw);
    #else
      return 0;
    #endif
  }
#endif // HAS_TEMP_HEATBREAK

#if HAS_TEMP_BOARD
  // Derived from RepRap FiveD extruder::getTemperature()
  // For ambient temperature measurement.
  float Temperature::analog_to_celsius_board(const int raw) {
    #if ENABLED(BOARD_USES_THERMISTOR)
      return marlin_temptable_lookup(TT_NAME(TEMP_SENSOR_BOARD), raw);
    #else
      return 0;
    #endif
  }
#endif // HAS_TEMP_BOARD

#if HAS_AC_CONTROLLER()

static void translate_ac_controller_faults(const char* pubby_name, ac_controller::Faults faults) {
  if (!faults) return;

  // Some faults are intentionally missing, because they shouldn't occur in normal printer operation.
  // We might as well BSOD when they do. No need to waste FLASH for them and error page would be pointless too.

  if (faults & ac_controller::Faults::RCD_TRIPPED) return fatal_error(ErrCode::ERR_ELECTRO_AC_CONTROLLER_RCD_TRIPPED);
  if (faults & ac_controller::Faults::POWERPANIC) return fatal_error(ErrCode::ERR_ELECTRO_AC_CONTROLLER_POWERPANIC);
  // ac_controller::Faults::OVERHEAT intentionally missing
  // Neither PSU NTC nor triac NTC are physically present on CORE One L version of AC controller
  if (faults & ac_controller::Faults::PSU_FAN_NOK) return fatal_error(ErrCode::ERR_ELECTRO_AC_CONTROLLER_PSU_FAN_NOK);
  // ac_controller::Faults::PSU_NTC_DISCONNECT intentionally missing
  // ac_controller::Faults::PSU_NTC_SHORT intentionally missing
  // PSU NTC is not physically present on CORE One L version of AC controller
  if (faults & ac_controller::Faults::BED_NTC_DISCONNECT) return fatal_error(ErrCode::ERR_TEMPERATURE_AC_CONTROLLER_BED_NTC_DISCONNECT);
  if (faults & ac_controller::Faults::BED_NTC_SHORT) return fatal_error(ErrCode::ERR_TEMPERATURE_AC_CONTROLLER_BED_NTC_SHORT);
  // ac_controller::Faults::TRIAC_NTC_DISCONNECT intentionally missing
  // ac_controller::Faults::TRIAC_NTC_SHORT intentionally missing
  // Triac NTC is not physically present on CORE One L version of AC controller
  if (faults & ac_controller::Faults::BED_FAN0_NOK) return fatal_error(ErrCode::ERR_ELECTRO_AC_CONTROLLER_BED_FAN0_NOK);
  if (faults & ac_controller::Faults::BED_FAN1_NOK) return fatal_error(ErrCode::ERR_ELECTRO_AC_CONTROLLER_BED_FAN1_NOK);
  // ac_controller::Faults::TRIAC_FAN_NOK intentionally missing
  // Triac fan is not physically present on CORE One L version of AC controller
  if (faults & ac_controller::Faults::GRID_NOK) return fatal_error(ErrCode::ERR_ELECTRO_AC_CONTROLLER_GRID_NOK);
  // ac_controller::Faults::CHAMBER_LOAD_NOK intentionally missing
  // Chamber heater is not physically present on CORE One L version of AC controller
  if (faults & ac_controller::Faults::BED_LOAD_NOK) return fatal_error(ErrCode::ERR_ELECTRO_AC_CONTROLLER_BED_LOAD_NOK);
  if (faults & ac_controller::Faults::PSU_NOK) return fatal_error(ErrCode::ERR_ELECTRO_AC_CONTROLLER_PSU_NOK);
  if (faults & ac_controller::Faults::BED_RUNAWAY) return fatal_error(ErrCode::ERR_TEMPERATURE_BED_THERMAL_RUNAWAY);
  if (faults & ac_controller::Faults::MCU_OVERHEAT) return fatal_error(ErrCode::ERR_TEMPERATURE_PUBBY_MCU_OVERHEAT, pubby_name);
  if (faults & ac_controller::Faults::PCB_OVERHEAT) return fatal_error(ErrCode::ERR_TEMPERATURE_PUBBY_PCB_OVERHEAT, pubby_name);
  if (faults & ac_controller::Faults::DATA_TIMEOUT) return fatal_error(ErrCode::ERR_ELECTRO_PUBBY_DATA_TIMEOUT, pubby_name);
  if (faults & ac_controller::Faults::HEARTBEAT_MISSING) return fatal_error(ErrCode::ERR_ELECTRO_PUBBY_HEARTBEAT_MISSING, pubby_name);
  // ac_controller::Faults::UNKNOWN intentionally missing
  // This fault is only ever triggered by development version of the AC controller

  // We still want to provide some details if the fault slips to production.
  bsod("%s faults=%" PRIu32, pubby_name, static_cast<uint32_t>(faults));
}

void translate_ac_controller_faults() {
  static constexpr const char* pubby_name = "AC controller";
  if (const auto faults = buddy::puppies::ac_controller.get_faults()) {
    translate_ac_controller_faults(pubby_name, *faults);
  } else {
    fatal_error(ErrCode::ERR_SYSTEM_PUPPY_NOT_RESPONDING, pubby_name);
  }
}
#endif

/**
 * Get the raw values into the actual temperatures.
 * The raw values are created in interrupt context,
 * and this function is called from normal context
 * as it would block the stepper routine.
 */
void Temperature::updateTemperaturesFromRawValues() {
  #if HAS_TOOLCHANGER()
    for (auto tool : PhysicalToolIndex::all()) {
      temp_hotend[tool].celsius = prusa_toolchanger.getTool(tool).get_hotend_temp();
    }
  #else
    for (auto tool : PhysicalToolIndex::all()) {
      temp_hotend[tool].celsius = analog_to_celsius_hotend(temp_hotend[tool].raw, tool);
    }
  #endif
  #if HAS_HEATED_BED
    #if HAS_MODULAR_BED()
      updateModularBedTemperature();
    #elif HAS_AC_CONTROLLER()
      translate_ac_controller_faults();
      temp_bed.celsius = buddy::puppies::ac_controller.get_bed_temp().value_or(0);
    #else
      temp_bed.celsius = analog_to_celsius_bed(temp_bed.raw);
    #endif

    uint32_t now_millis = millis();
    if (temp_bed.celsius > 0.0f) {
      if (bed_frame_est_celsius < 0.0f) {
        init_bed_frame_est_celsius();
      } else {
        float dt = (now_millis - bed_frame_millis) / 1000.0f;

        // A linear function that reaches estimated bed frame temperature after
        // about 150s for 60C and about 10 minutes for 100C if starting with a
        // cold bed. With a bed already partially warmed, the time is
        // proportionally shorter.
        float step = (0.06f + (100.0f - temp_bed.celsius) * 0.0015f) * dt;
        bed_frame_est_celsius += std::clamp(temp_bed.celsius - bed_frame_est_celsius, -step, step);
      }
    }

    bed_frame_millis = now_millis;
  #endif

  #if HAS_TEMP_HEATBREAK
    #if HAS_TOOLCHANGER()
      for (auto tool : PhysicalToolIndex::all()) {
        temp_heatbreak[tool].celsius = prusa_toolchanger.getTool(tool).get_heatbreak_temp();
      }
    #else
      for (auto tool : PhysicalToolIndex::all()) {
        temp_heatbreak[tool].celsius = analog_to_celsius_heatbreak(temp_heatbreak[tool].raw);
      }
    #endif
  #endif
  #if HAS_TEMP_BOARD
    temp_board.celsius = analog_to_celsius_board(temp_board.raw);
  #endif

  #if PRINTER_IS_PRUSA_iX()
    // Both psu and ambient temperatures use the MK4 bed thermistor
    temp_psu.celsius = scan_thermistor_table_bed(temp_psu.raw);
    temp_ambient.celsius = scan_thermistor_table_bed(temp_ambient.raw);
  #endif

  // Reset the watchdog on good temperature measurement
  watchdog_refresh();

  #if HAS_TOOLCHANGER()
  if(temp_bed.celsius == 0) {
    return; // Avoid marking reading as good when the bed temperature was not read
  }
  #endif

  temperatures_ready_state = true;
  temp_meas_ready = false;
}

#define _INIT_SOFT_FAN(P) OUT_WRITE(P, FAN_INVERTING ? LOW : HIGH)
#define INIT_FAN_PIN(P) do{ if (PWM_PIN(P)) SET_PWM(P); else _INIT_SOFT_FAN(P); }while(0)
#if EXTRUDER_AUTO_FAN_SPEED != 255
  #define INIT_E_AUTO_FAN_PIN(P) do{ if (P == FAN1_PIN || P == FAN2_PIN) { SET_PWM(P); } else SET_OUTPUT(P); }while(0)
#else
  #define INIT_E_AUTO_FAN_PIN(P) SET_OUTPUT(P)
#endif

/**
 * Initialize the temperature manager
 * The manager is implemented by periodic calls to manage_heater()
 */
void Temperature::init() {

  #if EARLY_WATCHDOG
    // Flag that the thermalManager should be running
    if (inited) return;
    inited = true;
  #endif

  #if BOTH(PIDTEMP, PID_EXTRUSION_SCALING)
    last_e_position = 0;
  #endif

  #if HAS_HEATER_0
    OUT_WRITE(HEATER_0_PIN, HEATER_0_INVERTING);
  #endif

  #if HAS_HEATER_1
    OUT_WRITE(HEATER_1_PIN, HEATER_1_INVERTING);
  #endif
  #if HAS_HEATER_2
    OUT_WRITE(HEATER_2_PIN, HEATER_2_INVERTING);
  #endif
  #if HAS_HEATER_3
    OUT_WRITE(HEATER_3_PIN, HEATER_3_INVERTING);
  #endif
  #if HAS_HEATER_4
    OUT_WRITE(HEATER_4_PIN, HEATER_4_INVERTING);
  #endif
  #if HAS_HEATER_5
    OUT_WRITE(HEATER_5_PIN, HEATER_5_INVERTING);
  #endif

  #if HAS_LOCAL_BED()
    WRITE_HEATER_BED(LOW);
  #endif

  #if HAS_FAN0
    INIT_FAN_PIN(FAN_PIN);
  #endif
  #if HAS_FAN1
    INIT_FAN_PIN(FAN1_PIN);
  #endif
  #if HAS_FAN2
    INIT_FAN_PIN(FAN2_PIN);
  #endif

  HAL_adc_init();

  #if HAS_TEMP_ADC_0
    HAL_ANALOG_SELECT(TEMP_0_PIN);
  #endif
  #if HAS_TEMP_ADC_1
    HAL_ANALOG_SELECT(TEMP_1_PIN);
  #endif
  #if HAS_TEMP_ADC_2
    HAL_ANALOG_SELECT(TEMP_2_PIN);
  #endif
  #if HAS_TEMP_ADC_3
    HAL_ANALOG_SELECT(TEMP_3_PIN);
  #endif
  #if HAS_TEMP_ADC_4
    HAL_ANALOG_SELECT(TEMP_4_PIN);
  #endif
  #if HAS_TEMP_ADC_5
    HAL_ANALOG_SELECT(TEMP_5_PIN);
  #endif
  #if PRINTER_IS_PRUSA_iX()
    HAL_ANALOG_SELECT(TEMP_PSU_PIN);
    HAL_ANALOG_SELECT(TEMP_AMBIENT_PIN);
  #endif
  #if HAS_TEMP_HEATBREAK
    HAL_ANALOG_SELECT(TEMP_HEATBREAK_PIN);
  #endif

  HAL_timer_start(TEMP_TIMER_NUM, TEMP_TIMER_FREQUENCY);
  ENABLE_TEMPERATURE_INTERRUPT();

  #if HAS_AUTO_FAN_0
    INIT_E_AUTO_FAN_PIN(E0_AUTO_FAN_PIN);
  #endif
  #if HAS_AUTO_FAN_1 && !_EFANOVERLAP(1,0)
    INIT_E_AUTO_FAN_PIN(E1_AUTO_FAN_PIN);
  #endif
  #if HAS_AUTO_FAN_2 && !(_EFANOVERLAP(2,0) || _EFANOVERLAP(2,1))
    INIT_E_AUTO_FAN_PIN(E2_AUTO_FAN_PIN);
  #endif
  #if HAS_AUTO_FAN_3 && !(_EFANOVERLAP(3,0) || _EFANOVERLAP(3,1) || _EFANOVERLAP(3,2))
    INIT_E_AUTO_FAN_PIN(E3_AUTO_FAN_PIN);
  #endif
  #if HAS_AUTO_FAN_4 && !(_EFANOVERLAP(4,0) || _EFANOVERLAP(4,1) || _EFANOVERLAP(4,2) || _EFANOVERLAP(4,3))
    INIT_E_AUTO_FAN_PIN(E4_AUTO_FAN_PIN);
  #endif
  #if HAS_AUTO_FAN_5 && !(_EFANOVERLAP(5,0) || _EFANOVERLAP(5,1) || _EFANOVERLAP(5,2) || _EFANOVERLAP(5,3) || _EFANOVERLAP(5,4))
    INIT_E_AUTO_FAN_PIN(E5_AUTO_FAN_PIN);
  #endif

  // Wait for temperature measurement to settle
  delay(250);

  #if HAS_HEATED_BED
    minmaxtemp_raw_BED = MarlinTemptableRawMinMax::compute(TT_NAME(THERMISTORBED), BED_MINTEMP, BED_MAXTEMP);
  #endif // HAS_HEATED_BED

  #if HAS_TEMP_HEATBREAK
    minmaxtemp_raw_HEATBREAK = MarlinTemptableRawMinMax::compute(heatbreak_temptable(), HEATBREAK_MINTEMP, HEATBREAK_MAXTEMP);
  #endif

  // At the end of init, load the temperatures
  updateTemperaturesFromRawValues();
}

#if WATCH_HOTENDS
  /**
   * Start Heating Sanity Check for hotends that are below
   * their target temperature by a configurable margin.
   * This is called when the temperature is set. (M104, M109)
   */
  void Temperature::start_watching_hotend(const uint8_t E_NAME) {
    const uint8_t ee = HOTEND_INDEX;
    if (degTargetHotend(ee) && degHotend(ee) < degTargetHotend(ee) - (WATCH_TEMP_INCREASE + TEMP_HYSTERESIS + 1)) {
      watch_hotend[ee].target = static_cast<int16_t>(degHotend(ee)) + WATCH_TEMP_INCREASE;
      watch_hotend[ee].next_ms = millis() + (WATCH_TEMP_PERIOD) * 1000UL;
    }
    else
      watch_hotend[ee].next_ms = 0;
  }
#endif

#if WATCH_BED
  /**
   * Start Heating Sanity Check for hotends that are below
   * their target temperature by a configurable margin.
   * This is called when the temperature is set. (M140, M190)
   */
  void Temperature::start_watching_bed() {
    if (degTargetBed() && degBed() < degTargetBed() - (WATCH_BED_TEMP_INCREASE + TEMP_BED_HYSTERESIS + 1)) {
      watch_bed.target = static_cast<int16_t>(degBed()) + WATCH_BED_TEMP_INCREASE;
      watch_bed.next_ms = millis() + (WATCH_BED_TEMP_PERIOD) * 1000UL;
    }
    else
      watch_bed.next_ms = 0;
  }
#endif

#if WATCH_HEATBREAK
  /**
   * Start cooling check for heatbreak that is above
   * its target temperature by a configurable margin.
   * This is called when the target temperature for heatbreak is set. 
   */
  void Temperature::start_watching_heatbreak(const uint8_t E_NAME) {
    const uint8_t ee = HOTEND_INDEX;

    // If the target temperature is set and the heatbreak is above the target + offset, keep watching the cooling
    if (degTargetHeatbreak(ee) > 0 && degHeatbreak(ee) > degTargetHeatbreak(ee) + HEATBREAK_MAXTEMP_OFFSET) {
      watch_heatbreak[ee].target = static_cast<int16_t>(degHeatbreak(ee)) - WATCH_HEATBREAK_TEMP_DECREASE;
      watch_heatbreak[ee].next_ms = millis() + (WATCH_HEATBREAK_TEMP_PERIOD) * 1000UL;
    }
    else {
      // We have reached the target temperature or the target temperature is not set -> stop watching
      watch_heatbreak[ee].next_ms = 0;
    }
  }

#endif

#if HAS_THERMAL_PROTECTION

  #if ENABLED(THERMAL_PROTECTION_HOTENDS)
    ThermalRunaway Temperature::thermal_runaway_hotends[HOTENDS];
  #endif
  #if HAS_THERMALLY_PROTECTED_BED
    ThermalRunaway Temperature::thermal_runaway_bed;
  #endif

  #if ENABLED(MODEL_DETECT_STUCK_THERMISTOR)

  int_least8_t Temperature::failed_cycles[HOTENDS] = {};

  /**
   * @brief Detect discrepancy between expected heating based on model and actual heating
   *
   * PWM output is checked once per 1 second. Each time it is THERMAL_PROTECTION_MODEL_DISCREPANCY
   * over feed_forward value failed_cycles are incremented. Each time it is under it is decremented.
   * Once failed_cycles reaches over THERMAL_PROTECTION_MODEL_PERIOD, temperature error is announced.
   *
   * @param pid_output heater PWM output
   * @param feed_forward part of the heater PWM output not affected by temperature readings
   * @param e hotend index
   */
  void Temperature::thermal_model_protection(const float& pid_output, const float& feed_forward, const uint8_t E_NAME) {
    const uint8_t ee = HOTEND_INDEX;

    // Zero initialize timers. Zero means not started.
    static millis_t timer[HOTENDS] = {};
    // Expected interval is 1000 ms. min_interval_ms set to 100 ms, so it will be visible in samples collected if
    // expected interval doesn't hold.
    METRIC_DEF(heating_model_discrepancy, "heating_model_discrepancy", METRIC_VALUE_INTEGER, 100, METRIC_DISABLED);

    // Start the timer if already not started. In case millis() == 0 it will not start the timer.
    // But it will do no harm, as it will be started in the next call to this function.
    if(!timer[ee]) timer[ee] = millis();

    //Each 1 second
    if (ELAPSED(millis(), timer[ee]))
    {
      timer[ee] = millis() + 1000UL;

      float work_feed_forward = feed_forward;
      // Ignore extreme model forecasts caused by extrusion
      // scaling during un/retractions.
      LIMIT(work_feed_forward, 0, PID_MAX);
      const float model_discrepancy = pid_output - work_feed_forward;
      metric_record_integer(&heating_model_discrepancy, static_cast<int>(model_discrepancy));

      if (model_discrepancy > THERMAL_PROTECTION_MODEL_DISCREPANCY) ++failed_cycles[ee];
      else --failed_cycles[ee];

      if (failed_cycles[ee] < 0) failed_cycles[ee] = 0;
      if (failed_cycles[ee] > THERMAL_PROTECTION_MODEL_PERIOD + self_healing_cycles) {
          failed_cycles[ee] = THERMAL_PROTECTION_MODEL_PERIOD + self_healing_cycles;
      }
    }
  }
  #endif

#endif // HAS_THERMAL_PROTECTION

void Temperature::disable_all_heaters() {
    disable_heaters(disable_bed_t::yes);
}

void Temperature::disable_hotend() {
    disable_heaters(disable_bed_t::no);

}

void Temperature::disable_local_heaters() {
#if HAS_DWARF() && HAS_REMOTE_BED() && !HAS_LOCAL_BED()
    // No local heater present
#elif !HAS_DWARF() && HAS_REMOTE_BED() && !HAS_LOCAL_BED()
    disable_hotend();
#elif !HAS_DWARF() && !HAS_REMOTE_BED() && HAS_LOCAL_BED()
    disable_all_heaters();
#else
#if BOARD_IS_DWARF()
  disable_all_heaters();
#else
  #error
#endif
#endif
}

void Temperature::disable_heaters(Temperature::disable_bed_t disable_bed) {

  #if ENABLED(AUTOTEMP)
    planner.autotemp_enabled = false;
  #endif

  for (auto tool : PhysicalToolIndex::all()) {
    setTargetHotend(0, tool);
  }

  #if HAS_HEATED_BED
    if (disable_bed == disable_bed_t::yes){
      setTargetBed(0);
      #if HAS_LOCAL_BED()
      temp_bed.soft_pwm_amount = 0;
      WRITE_HEATER_BED(LOW);
      #endif
    }
  #endif

  #define DISABLE_HEATER(NR) { \
    setTargetHotend(0, NR); \
    temp_hotend[NR].soft_pwm_amount = 0; \
    WRITE_HEATER_ ##NR (LOW); \
  }

  #if HAS_TEMP_HOTEND
    DISABLE_HEATER(0);
    #if HOTENDS > 1
      DISABLE_HEATER(1);
      #if HOTENDS > 2
        DISABLE_HEATER(2);
        #if HOTENDS > 3
          DISABLE_HEATER(3);
          #if HOTENDS > 4
            DISABLE_HEATER(4);
            #if HOTENDS > 5
              DISABLE_HEATER(5);
            #endif // HOTENDS > 5
          #endif // HOTENDS > 4
        #endif // HOTENDS > 3
      #endif // HOTENDS > 2
    #endif // HOTENDS > 1
  #endif
}

/**
 * Get raw temperatures
 */
void Temperature::set_current_temp_raw() {

  #if HAS_TEMP_ADC_0
    temp_hotend[0].update();
  #endif

  #if HAS_TEMP_ADC_1
      temp_hotend[1].update();
    #if HAS_TEMP_ADC_2
      temp_hotend[2].update();
      #if HAS_TEMP_ADC_3
        temp_hotend[3].update();
        #if HAS_TEMP_ADC_4
          temp_hotend[4].update();
          #if HAS_TEMP_ADC_5
            temp_hotend[5].update();
          #endif // HAS_TEMP_ADC_5
        #endif // HAS_TEMP_ADC_4
      #endif // HAS_TEMP_ADC_3
    #endif // HAS_TEMP_ADC_2
  #endif // HAS_TEMP_ADC_1

  #if HAS_HEATED_BED
    temp_bed.update();
  #endif

  #if HAS_TEMP_HEATBREAK
    for (auto tool : PhysicalToolIndex::all()) {
      temp_heatbreak[tool].update();
    }
  #endif

  #if HAS_TEMP_BOARD
    temp_board.update();
  #endif

  #if PRINTER_IS_PRUSA_iX()
    temp_psu.update();
    temp_ambient.update();
  #endif

  temp_meas_ready = true;
}

void Temperature::readings_ready() {

  // Update the raw values if they've been read. Else we could be updating them during reading.
  if (!temp_meas_ready) set_current_temp_raw();

  for (auto tool : PhysicalToolIndex::all()) {
    temp_hotend[tool].reset();
  }

  #if HAS_HEATED_BED
    temp_bed.reset();
  #endif

  #if HAS_TEMP_HEATBREAK
    for (auto tool : PhysicalToolIndex::all()) {
      temp_heatbreak[tool].reset();
    }
  #endif

  #if HAS_TEMP_BOARD
    temp_board.reset();
  #endif

  #if PRINTER_IS_PRUSA_iX()
    temp_psu.reset();
    temp_ambient.reset();
  #endif

  for (auto tool : PhysicalToolIndex::all()) {
    const auto e = tool.to_raw();

    const bool heater_on = (temp_hotend[e].target > 0
      #if ENABLED(PIDTEMP)
        || temp_hotend[e].soft_pwm_amount > 0
      #endif
    );
  #if HAS_TOOLCHANGER()
    // Toolchanger doesn't report raw
    if (temp_hotend[e].celsius > temp_range[e].maxtemp) {
      max_temp_error((heater_ind_t)e);
    }
    if (heater_on && temp_hotend[e].celsius < temp_range[e].mintemp) {
      min_temp_error((heater_ind_t)e);
    }
  #else
    temp_range[e].raw.check_temperror(temp_hotend[e].raw, e, heater_on);
  #endif
  }

  #if HAS_HEATED_BED
    #if HAS_REMOTE_BED()
      //With remote bed we get temperatures in °C from controller. No raw values to check.
    #endif
    #if HAS_LOCAL_BED()
    const bool bed_on = (temp_bed.target > 0)
      #if ENABLED(PIDTEMPBED)
        || (temp_bed.soft_pwm_amount > 0)
      #endif
    ;
      minmaxtemp_raw_BED.check_temperror(temp_bed.raw, H_BED, bed_on);
    #endif
  #endif

  #if HAS_TEMP_HEATBREAK
    #if !HAS_TOOLCHANGER()
    for (auto tool : PhysicalToolIndex::all()) {
        //const bool chamber_on = (temp_chamber.target > 0);
        const bool heater_on = (temp_hotend[tool].target > 0
                                #if ENABLED(PIDTEMP)
                                || temp_hotend[tool].soft_pwm_amount > 0
                                #endif
        );
        minmaxtemp_raw_HEATBREAK.check_temperror(temp_heatbreak[tool].raw, H_HEATBREAK_FIRST + tool.to_raw(), heater_on);
    }
    #endif
  #endif
}

/**
 * Timer 0 is shared with millies so don't change the prescaler.
 *
 * On AVR this ISR uses the compare method so it runs at the base
 * frequency (16 MHz / 64 / 256 = 976.5625 Hz), but at the TCNT0 set
 * in OCR0B above (128 or halfway between OVFs).
 *
 *  - Manage PWM to all the heaters and fan
 *  - Prepare or Measure one of the raw ADC sensor values
 *  - Check new temperature values for MIN/MAX errors (kill on error)
 *  - Step the babysteps value for each axis towards 0
 *  - For ENDSTOP_INTERRUPTS_FEATURE check endstops if flagged
 *  - Call planner.tick to count down its "ignore" time
 */
HAL_TEMP_TIMER_ISR() {
  HAL_timer_isr_prologue(TEMP_TIMER_NUM);

#if (BOARD_IS_XBUDDY())
    AdcGet::sampleNozzle();
#endif
    Temperature::isr();

  HAL_timer_isr_epilogue(TEMP_TIMER_NUM);
}

class SoftPWM {
public:
  uint8_t count;
  inline bool add(const uint8_t mask, const uint8_t amount) {
    count = (count & mask) + amount; return (count > mask);
  }
};

void Temperature::isr() {

  static int8_t temp_count = -1;
  static ADCSensorState adc_sensor_state = StartupDelay;

  #if DISABLED(HW_PWM_HEATERS)
    static uint8_t pwm_count = _BV(SOFT_PWM_SCALE);
    // avoid multiple loads of pwm_count
    uint8_t pwm_count_tmp = pwm_count;

    static SoftPWM soft_pwm_hotend[HOTENDS];

    #if HAS_LOCAL_BED()
      static SoftPWM soft_pwm_bed;
    #endif

      constexpr uint8_t pwm_mask =
        #if ENABLED(SOFT_PWM_DITHER)
          _BV(SOFT_PWM_SCALE) - 1
        #else
          0
        #endif
      ;
      #define _PWM_MOD(N,S,T) do{                           \
        const bool on = S.add(pwm_mask, T.soft_pwm_amount); \
        WRITE_HEATER_##N(on);                               \
      }while(0)

      /**
       * Standard heater PWM modulation
       */
      if (pwm_count_tmp >= 127) {
        pwm_count_tmp -= 127;

        #define _PWM_MOD_E(N) _PWM_MOD(N,soft_pwm_hotend[N],temp_hotend[N])
        _PWM_MOD_E(0);
        #if HOTENDS > 1
          _PWM_MOD_E(1);
          #if HOTENDS > 2
            _PWM_MOD_E(2);
            #if HOTENDS > 3
              _PWM_MOD_E(3);
              #if HOTENDS > 4
                _PWM_MOD_E(4);
                #if HOTENDS > 5
                  _PWM_MOD_E(5);
                #endif // HOTENDS > 5
              #endif // HOTENDS > 4
            #endif // HOTENDS > 3
          #endif // HOTENDS > 2
        #endif // HOTENDS > 1

        #if HAS_LOCAL_BED()
          _PWM_MOD(BED,soft_pwm_bed,temp_bed);
        #endif
      }
      else {
        #define _PWM_LOW(N,S) do{ if (S.count <= pwm_count_tmp) WRITE_HEATER_##N(LOW); }while(0)
        #define _PWM_LOW_E(N) _PWM_LOW(N, soft_pwm_hotend[N])
        _PWM_LOW_E(0);
        #if HOTENDS > 1
          _PWM_LOW_E(1);
          #if HOTENDS > 2
            _PWM_LOW_E(2);
            #if HOTENDS > 3
              _PWM_LOW_E(3);
              #if HOTENDS > 4
                _PWM_LOW_E(4);
                #if HOTENDS > 5
                  _PWM_LOW_E(5);
                #endif // HOTENDS > 5
              #endif // HOTENDS > 4
            #endif // HOTENDS > 3
          #endif // HOTENDS > 2
        #endif // HOTENDS > 1

        #if HAS_LOCAL_BED()
          _PWM_LOW(BED, soft_pwm_bed);
        #endif
      }

      // SOFT_PWM_SCALE to frequency:
      //
      // 0: 16000000/64/256/128 =   7.6294 Hz
      // 1:                / 64 =  15.2588 Hz
      // 2:                / 32 =  30.5176 Hz
      // 3:                / 16 =  61.0352 Hz
      // 4:                /  8 = 122.0703 Hz
      // 5:                /  4 = 244.1406 Hz
      pwm_count = pwm_count_tmp + _BV(SOFT_PWM_SCALE);

  #endif // HW_PWM_HEATERS


  /**
   * One sensor is sampled on every other call of the ISR.
   * Each sensor is read 16 (OVERSAMPLENR) times, taking the average.
   *
   * On each Prepare pass, ADC is started for a sensor pin.
   * On the next pass, the ADC value is read and accumulated.
   *
   * This gives each ADC 0.9765ms to charge up.
   */
  #define ACCUMULATE_ADC(obj) do{ \
    if (!HAL_ADC_READY()) next_sensor_state = adc_sensor_state; \
    else obj.sample(HAL_READ_ADC()); \
  }while(0)

  ADCSensorState next_sensor_state = adc_sensor_state < SensorsReady ? (ADCSensorState)(int(adc_sensor_state) + 1) : StartSampling;

  switch (adc_sensor_state) {

    case SensorsReady: {
      // All sensors have been read. Stay in this state for a few
      // ISRs to save on calls to temp update/checking code below.
      constexpr int8_t extra_loops = MIN_ADC_ISR_LOOPS - (int8_t)SensorsReady;
      static uint8_t delay_count = 0;
      if (extra_loops > 0) {
        if (delay_count == 0) delay_count = extra_loops;  // Init this delay
        if (--delay_count)                                // While delaying...
          next_sensor_state = SensorsReady;               // retain this state (else, next state will be 0)
        break;
      }
      else {
        adc_sensor_state = StartSampling;                 // Fall-through to start sampling
        next_sensor_state = (ADCSensorState)(int(StartSampling) + 1);
      }
    }

    case StartSampling:                                   // Start of sampling loops. Do updates/checks.
      if (++temp_count >= OVERSAMPLENR) {                 // 10 * 16 * 1/(16000000/64/256)  = 164ms.
        temp_count = 0;
        readings_ready();
      }
      break;

    #if HAS_TEMP_ADC_0
      case PrepareTemp_0: HAL_START_ADC(TEMP_0_PIN); break;
      case MeasureTemp_0: ACCUMULATE_ADC(temp_hotend[0]); break;
    #endif

    #if HAS_LOCAL_BED()
      case PrepareTemp_BED: break;
      case MeasureTemp_BED: temp_bed.sample(analogRead_TEMP_BED()); break;
    #endif

    #if HAS_TEMP_HEATBREAK
      case PrepareTemp_HEATBREAK: HAL_START_ADC(TEMP_HEATBREAK_PIN); break;
      case MeasureTemp_HEATBREAK: ACCUMULATE_ADC(temp_heatbreak[0]); break;
    #endif

    #if HAS_TEMP_BOARD
      case PrepareTemp_BOARD: HAL_START_ADC(TEMP_BOARD_PIN); break;
      case MeasureTemp_BOARD: ACCUMULATE_ADC(temp_board); break;
    #endif


    #if PRINTER_IS_PRUSA_iX()
      case PrepareTemp_PSU: HAL_START_ADC(TEMP_PSU_PIN); break;
      case MeasureTemp_PSU: ACCUMULATE_ADC(temp_psu); break;
      case PrepareTemp_AMBIENT: HAL_START_ADC(TEMP_AMBIENT_PIN); break;
      case MeasureTemp_AMBIENT: ACCUMULATE_ADC(temp_ambient); break;
    #endif

    #if HAS_TEMP_ADC_1
      case PrepareTemp_1: HAL_START_ADC(TEMP_1_PIN); break;
      case MeasureTemp_1: ACCUMULATE_ADC(temp_hotend[1]); break;
    #endif

    #if HAS_TEMP_ADC_2
      case PrepareTemp_2: HAL_START_ADC(TEMP_2_PIN); break;
      case MeasureTemp_2: ACCUMULATE_ADC(temp_hotend[2]); break;
    #endif

    #if HAS_TEMP_ADC_3
      case PrepareTemp_3: HAL_START_ADC(TEMP_3_PIN); break;
      case MeasureTemp_3: ACCUMULATE_ADC(temp_hotend[3]); break;
    #endif

    #if HAS_TEMP_ADC_4
      case PrepareTemp_4: HAL_START_ADC(TEMP_4_PIN); break;
      case MeasureTemp_4: ACCUMULATE_ADC(temp_hotend[4]); break;
    #endif

    #if HAS_TEMP_ADC_5
      case PrepareTemp_5: HAL_START_ADC(TEMP_5_PIN); break;
      case MeasureTemp_5: ACCUMULATE_ADC(temp_hotend[5]); break;
    #endif

    case StartupDelay: break;

  } // switch(adc_sensor_state)

  // Go to the next state
  adc_sensor_state = next_sensor_state;

  //
  // Additional ~1KHz Tasks
  //

  #if ENABLED(BABYSTEPPING)
    babystep.task();
  #endif

  #if HAS_PLANNER()
    // Poll endstops state, if required
    endstops.poll();

    // Periodically call the planner timer
    planner.tick();
  #endif /* HAS_PLANNER() */
}

#if HAS_TEMP_SENSOR

  #include "../gcode/gcode.h"

  static void print_heater_state(const float &c, const float &t, const heater_ind_t e=INDEX_NONE) {
    char k;
    int8_t tool_nr = -1;
    #if HAS_TEMP_HOTEND
    if (e == INDEX_NONE) k = 'T';
    if (e >= H_NOZZLE_FIRST && e <= H_NOZZLE_LAST) {
      k = 'T';
      tool_nr = e - H_NOZZLE_FIRST;
    }
    #endif
    #if HAS_HEATED_BED
    if (e == H_BED) k = 'B';
    #endif
    #if HAS_TEMP_BOARD
      if (e == H_BOARD) k = 'A';
    #endif
    #if HAS_TEMP_HEATBREAK
      if (e >= H_HEATBREAK_FIRST && e <= H_HEATBREAK_LAST){
        k = 'X';
        tool_nr = e - H_HEATBREAK_FIRST;
      }
    #endif

    SERIAL_CHAR(' ');
    SERIAL_CHAR(k);
    #if HOTENDS > 1
      if (tool_nr >= 0) SERIAL_CHAR('0' + tool_nr);
    #else
      UNUSED(tool_nr);
    #endif
    SERIAL_CHAR(':');
    SERIAL_ECHO(c);
    SERIAL_ECHOPAIR("/" , t);
    delay(2);
  }

  void Temperature::print_heater_states(const uint8_t target_extruder) {
    #if HAS_TEMP_HOTEND
      print_heater_state(degHotend(target_extruder), degTargetHotend(target_extruder));
    #endif
    #if HAS_HEATED_BED
      print_heater_state(degBed(), degTargetBed(), H_BED);
    #endif

    #if HAS_TEMP_HEATBREAK
      print_heater_state(degHeatbreak(target_extruder)
          , degTargetHeatbreak(target_extruder)
        , (heater_ind_t) (H_HEATBREAK_FIRST + target_extruder)
      );
    #endif // HAS_TEMP_HEATBREAK

    #if HAS_TEMP_BOARD
      print_heater_state(degBoard()
          , 0
        , H_BOARD
      );
    #endif // HAS_TEMP_BOARD

    #if HOTENDS > 1
      for (auto tool : PhysicalToolIndex::all()) {
        print_heater_state(degHotend(tool), degTargetHotend(tool), (heater_ind_t)tool.to_raw());
      }
    #endif
    SERIAL_ECHOPAIR(" @:", getHeaterPower((heater_ind_t)target_extruder));
    #if HAS_HEATED_BED
      SERIAL_ECHOPAIR(" B@:", getHeaterPower(H_BED));
    #endif
    #if HAS_CHAMBER_API()
      auto current_chamber_temperature = buddy::chamber().current_temperature();
      if (current_chamber_temperature.has_value()) SERIAL_ECHOPAIR(" C@:", current_chamber_temperature.value());
    #endif

    #if HAS_TEMP_HEATBREAK
      SERIAL_ECHOPAIR(" HBR@:", getHeaterPower((heater_ind_t)(H_HEATBREAK_FIRST + target_extruder)));
    #endif
    #if HOTENDS > 1
      for (auto tool : PhysicalToolIndex::all()) {
        SERIAL_ECHOPAIR(" @", tool.to_raw());
        SERIAL_CHAR(':');
        SERIAL_ECHO(getHeaterPower((heater_ind_t)tool.to_raw()));
      }
    #endif

    // Detailed modular bed report
    #if HAS_MODULAR_BED()
      for(int y = 0; y < Y_HBL_COUNT; ++y) {
        for(int x = 0; x < X_HBL_COUNT; ++x) {
          SERIAL_ECHO(" B_");
          SERIAL_ECHO(x);
          SERIAL_CHAR('_');
          SERIAL_ECHO(y);
          SERIAL_CHAR(':');
          SERIAL_ECHO(advanced_modular_bed->get_temp(x, y));
          SERIAL_CHAR('/');
          SERIAL_ECHO(advanced_modular_bed->get_target(x, y));
          SERIAL_FLUSH();
        }
      }
    #endif
  }

  #if ENABLED(AUTO_REPORT_TEMPERATURES)

    uint8_t Temperature::auto_report_temp_interval;
    millis_t Temperature::next_temp_report_ms;

    void Temperature::auto_report_temperatures() {
      if (auto_report_temp_interval && ELAPSED(millis(), next_temp_report_ms)) {
        // Do not log heater states, only print to serial
        SerialLoggingDisabler sld;

        next_temp_report_ms = millis() + 1000UL * auto_report_temp_interval;
        PORT_REDIRECT(SERIAL_BOTH);
        print_heater_states(active_extruder);
        SERIAL_EOL();
      }
    }

  #endif // AUTO_REPORT_TEMPERATURES

  #if HAS_TEMP_HOTEND

    #ifndef MIN_COOLING_SLOPE_DEG
      #define MIN_COOLING_SLOPE_DEG 1.50
    #endif
    #ifndef MIN_COOLING_SLOPE_TIME
      #define MIN_COOLING_SLOPE_TIME 60
    #endif

    bool Temperature::is_hotend_temperature_reached(uint8_t hotend) {
      return degTargetHotend(hotend) <= 0 || (temp_hotend_residency_start_ms[hotend] && !PENDING(millis(), temp_hotend_residency_start_ms[hotend] + (TEMP_RESIDENCY_TIME) * 1000UL));
    }

    bool Temperature::are_hotend_temperatures_reached() {
      for (auto tool : PhysicalToolIndex::all()) {
        if (!is_hotend_temperature_reached(tool)) {
            return false;
        }
      }

      return true;
    }

    void Temperature::setTargetHotend(const int16_t celsius, const uint8_t E_NAME) {
    #if BOARD_IS_MASTER_BOARD()
        // We cannot overwrite target temps while the safety_timer is active, deactivate it first
        buddy::safety_timer().reset_restore_nonblocking();
    #endif

        const uint8_t ee = HOTEND_INDEX;
        const int16_t new_temp = _MIN(celsius, temp_range[ee].maxtemp - HEATER_MAXTEMP_SAFETY_MARGIN);

    #if ENABLED(AUTO_POWER_CONTROL)
        if (celsius) {
            powerManager.power_on();
        }
    #endif

        // target changed, reset time when it reached target
        if (temp_hotend[ee].target != new_temp) {
            temp_hotend_residency_start_ms[ee] = 0;
        }

        temp_hotend[ee].target = new_temp;
    #if BOARD_IS_MASTER_BOARD()
        // This is a legit use
        marlin_server::call_manually::set_temp_to_display(new_temp, ee);
    #endif

        start_watching_hotend(ee);
    #if HAS_TOOLCHANGER()
        prusa_toolchanger.getTool(ee).set_hotend_target_temp(temp_hotend[ee].target);
    #endif
    }

    bool Temperature::wait_for_hotend(const uint8_t target_extruder, const bool no_wait_for_cooling/*=true*/, bool fan_cooling/*=false*/) {
      #if BOARD_IS_MASTER_BOARD()
        // Keep all heaters on while we're waiting for temperatures
        buddy::SafetyTimerBlocker safety_timer_blocker;
      #endif

      // Loop until the temperature has stabilized

      #if DISABLED(BUSY_WHILE_HEATING) && ENABLED(HOST_KEEPALIVE_FEATURE)
        KEEPALIVE_STATE(NOT_BUSY);
      #endif

      float target_temp = -1.0, old_temp = 9999.0;
      bool wants_to_cool = false;
      wait_for_heatup = true;
      millis_t now, next_temp_ms = 0, next_cool_check_ms = 0;

      /// !!! PRINT FAN IS ALWAYS FAN 0
      const uint8_t fan_speed_at_start = get_fan_speed(0);
      ScopeGuard fan_restore_guard = [&] {
        thermalManager.set_fan_speed(0, fan_speed_at_start);
      };

      PrintStatusMessageGuard statusGuard;

      do {
        #if HAS_PLANNER()
          // Check if we're aborting
          if (planner.draining()) break;
        #endif

        // Target temperature might be changed during the loop
        if (target_temp != degTargetHotend(target_extruder)) {
          wants_to_cool = temp_hotend[target_extruder].target < temp_hotend[target_extruder].celsius;
          target_temp = degTargetHotend(target_extruder);

          // Exit if S<lower>, continue if S<higher>, R<lower>, or R<higher>
          if (no_wait_for_cooling && wants_to_cool) break;

          // If fan_cooling is enabled, assist the cooling/heating with the print fan
          // !!! ONLY WORKS FOR ACTIVE EXTRUDER - PRINT FAN IS ALWAYS FAN 0
          if (fan_cooling && active_extruder == target_extruder)
            thermalManager.set_fan_speed(0, wants_to_cool ? 255 : 0);
        }

        now = millis();
        if (ELAPSED(now, next_temp_ms)) { // Print temp & remaining time every 1s while waiting
          // Do not log heater states, only print to serial
          SerialLoggingDisabler sld;

          next_temp_ms = now + 1000UL;
          print_heater_states(target_extruder);
          SERIAL_ECHOPGM(" W:");
          if (temp_hotend_residency_start_ms[target_extruder]) {
              if (!is_hotend_temperature_reached(target_extruder)){
                SERIAL_ECHO(long((((TEMP_RESIDENCY_TIME) * 1000UL) - (now - temp_hotend_residency_start_ms[target_extruder])) / 1000UL));
              } else {
                SERIAL_CHAR('0');
              }
          } else
            SERIAL_CHAR('?');
          SERIAL_EOL();
        }

        idle(true);

        const float temp = degHotend(target_extruder);
        statusGuard.update<PrintStatusMessage::waiting_for_hotend_temp>({.progress{ .current = temp, .target = target_temp }, .tool=target_extruder});

        // Prevent a wait-forever situation if R is misused i.e. M109 R0
        if (wants_to_cool) {
          // break after MIN_COOLING_SLOPE_TIME seconds
          // if the temperature did not drop at least MIN_COOLING_SLOPE_DEG
          if (!next_cool_check_ms || ELAPSED(now, next_cool_check_ms)) {
            if (old_temp - temp < float(MIN_COOLING_SLOPE_DEG)) break;
            next_cool_check_ms = now + 1000UL * MIN_COOLING_SLOPE_TIME;
            old_temp = temp;
          }
        }
      } while (wait_for_heatup && !is_hotend_temperature_reached(target_extruder));

      return wait_for_heatup;
    }

  #endif // HAS_TEMP_HOTEND

  #if HAS_HEATED_BED

    #ifndef MIN_COOLING_SLOPE_DEG_BED
      #define MIN_COOLING_SLOPE_DEG_BED 1.50
    #endif
    #ifndef MIN_COOLING_SLOPE_TIME_BED
      #define MIN_COOLING_SLOPE_TIME_BED 60
    #endif

    bool Temperature::is_bed_temperature_reached() {
      // TODO: Switch to residency time and employ with wait_for_bed
      // To achieve that, we will have to take the residency out of wait_for_bed and make it global
      return temp_bed.target <= 0 || std::abs(temp_bed.target - temp_bed.celsius) <= TEMP_BED_HYSTERESIS;
    }

    void Temperature::setTargetBed(const int16_t celsius) {
      // We cannot overwrite target temps while the safety_timer is active, deactivate it first
      buddy::safety_timer().reset_restore_nonblocking();

    #if ENABLED(AUTO_POWER_CONTROL)
        if (celsius) {
            powerManager.power_on();
        }
    #endif
        temp_bed.target =
    #ifdef BED_MAXTEMP
            _MIN(celsius, BED_MAXTEMP - BED_MAXTEMP_SAFETY_MARGIN)
    #else
            celsius
    #endif
            ;

    #if HAS_MODULAR_BED()
        for (uint8_t x = 0; x < X_HBL_COUNT; ++x) {
            for (uint8_t y = 0; y < Y_HBL_COUNT; ++y) {
                int16_t target_temp = 0;
                if (temp_bed.enabled_mask & (1 << advanced_modular_bed->idx(x, y))) {
                    target_temp = temp_bed.target;
                }
                advanced_modular_bed->set_target(x, y, target_temp);
            }
        }
        advanced_modular_bed->update_bedlet_temps(temp_bed.enabled_mask, temp_bed.target);
    #endif

    #if HAS_AC_CONTROLLER()
        buddy::puppies::ac_controller.set_bed_target_temp(temp_bed.target);
    #endif

        start_watching_bed();
    }


    bool Temperature::wait_for_bed(const bool no_wait_for_cooling/*=true*/) {
      // TODO: Employ is_bed_temperature_reached once it considers residency
      
      // Keep all heaters on while we're waiting for temperatures
      buddy::SafetyTimerBlocker safety_timer_blocker;

      #if TEMP_BED_RESIDENCY_TIME > 0
        millis_t residency_start_ms = 0;
        bool first_loop = true;
        // Loop until the temperature has stabilized
        #define TEMP_BED_CONDITIONS (!residency_start_ms || PENDING(now, residency_start_ms + (TEMP_BED_RESIDENCY_TIME) * 1000UL))
      #else
        // Loop until the temperature is very close target
        #define TEMP_BED_CONDITIONS (wants_to_cool ? isCoolingBed() : isHeatingBed())
      #endif

      float target_temp = -1, old_temp = 9999;
      bool wants_to_cool = false;
      wait_for_heatup = true;
      millis_t now, next_temp_ms = 0, next_cool_check_ms = 0;

      #if DISABLED(BUSY_WHILE_HEATING) && ENABLED(HOST_KEEPALIVE_FEATURE)
        KEEPALIVE_STATE(NOT_BUSY);
      #endif

      PrintStatusMessageGuard statusGuard;

      do {
        // Check if we're aborting
        if (planner.draining()) break;

        // Target temperature might be changed during the loop
        if (target_temp != degTargetBed()) {
          wants_to_cool = isCoolingBed();
          target_temp = degTargetBed();

          // Exit if S<lower>, continue if S<higher>, R<lower>, or R<higher>
          if (no_wait_for_cooling && wants_to_cool) break;
        }

        now = millis();
        if (ELAPSED(now, next_temp_ms)) { //Print Temp Reading every 1 second while heating up.
          // Do not log heater states, only print to serial
          SerialLoggingDisabler sld;

          next_temp_ms = now + 1000UL;
          print_heater_states(active_extruder);
          #if TEMP_BED_RESIDENCY_TIME > 0
            SERIAL_ECHOPGM(" W:");
            if (residency_start_ms)
              SERIAL_ECHO(long((((TEMP_BED_RESIDENCY_TIME) * 1000UL) - (now - residency_start_ms)) / 1000UL));
            else
              SERIAL_CHAR('?');
          #endif
          SERIAL_EOL();
        }

        idle(true);

        const float temp = degBed();
        statusGuard.update<PrintStatusMessage::waiting_for_bed_temp>({.current = temp, .target = target_temp});

        #if TEMP_BED_RESIDENCY_TIME > 0

          const float temp_diff = ABS(target_temp - temp);

          if (!residency_start_ms) {
            // Start the TEMP_BED_RESIDENCY_TIME timer when we reach target temp for the first time.
            if (temp_diff < TEMP_BED_WINDOW) {
              residency_start_ms = now;
              if (first_loop) residency_start_ms += (TEMP_BED_RESIDENCY_TIME) * 1000UL;
            }
          }
          else if (temp_diff > TEMP_BED_HYSTERESIS) {
            // Restart the timer whenever the temperature falls outside the hysteresis.
            residency_start_ms = now;
          }

        #endif // TEMP_BED_RESIDENCY_TIME > 0

        // Prevent a wait-forever situation if R is misused i.e. M190 R0
        if (wants_to_cool) {
          // Break after MIN_COOLING_SLOPE_TIME_BED seconds
          // if the temperature did not drop at least MIN_COOLING_SLOPE_DEG_BED
          if (!next_cool_check_ms || ELAPSED(now, next_cool_check_ms)) {
            if (old_temp - temp < float(MIN_COOLING_SLOPE_DEG_BED)) break;
            next_cool_check_ms = now + 1000UL * MIN_COOLING_SLOPE_TIME_BED;
            old_temp = temp;
          }
        }

        #if TEMP_BED_RESIDENCY_TIME > 0
          first_loop = false;
        #endif

      } while (wait_for_heatup && TEMP_BED_CONDITIONS);

      return wait_for_heatup;
    }

    void Temperature::init_bed_frame_est_celsius() {
        if (temp_bed.celsius < room_temperature) {
          // If around room temperature, init directly to bed temperature
          bed_frame_est_celsius = temp_bed.celsius;
        } else {
          // If over room temp, init with a fraction of the current temp that's
          // over room temperature, as a crude estimation of how the bed frame
          // has been heated up
          bed_frame_est_celsius = room_temperature + (temp_bed.celsius - room_temperature) * 0.7f;
        }
    }

    void Temperature::wait_for_frame_heatup() {
        // Keep everything heated up when absorbing heat
        buddy::SafetyTimerBlocker safety_timer_blocker;
      
        if (fabs(temp_bed.target - bed_frame_est_celsius) < 0.5f) {
            log_info(MarlinServer, "Absorbing heat: already stable, continuing");
            return;
        }

        if (marlin_debug_flags & MARLIN_DEBUG_DRYRUN) {
            // In dry run, the bed is left cold. The temperature would never stabilize.
            return;
        }

        if (temp_bed.target < bed_frame_est_celsius) {
          // Do not wait for cooldown. Cooling is slow and propagated evenly across the bed, it won't warp the bed differently.
          log_info(MarlinServer, "Absorbing heat: target lower than actual temp, continuing");
          return;
        }

        if (temp_bed.target <= room_temperature) {
          log_info(MarlinServer, "Absorbing heat: target lower than room temp, continuing");
          return;
        }

        SkippableGCode::Guard skippable_operation;
        PrintStatusMessageGuard status_guard;

        float start_target = temp_bed.target;
        float start_diff = fabs(start_target - bed_frame_est_celsius);
        while (fabs(temp_bed.target - bed_frame_est_celsius) > 0.5f && !skippable_operation.is_skip_requested()) {
            // Check if we're aborting
            if (planner.draining()) {
                break;
            }
            if (start_target != temp_bed.target) {
              //Target changed -> recalculate start_diff
              start_target = temp_bed.target;
              start_diff = fabs(start_target - bed_frame_est_celsius);
            }

            idle(true);

            auto progress = std::clamp(100 - (fabs(temp_bed.target - bed_frame_est_celsius) / start_diff) * 100, 0.f, 100.f);

            status_guard.update<PrintStatusMessage::absorbing_heat>({ .current = progress, .target = 100 });
        }

        MarlinUI::reset_status();
    }

  #endif // HAS_HEATED_BED

#endif // HAS_TEMP_SENSOR

#if HAS_MODULAR_BED()
void Temperature::updateModularBedTemperature(){
      float sum = 0;
      uint8_t count = 0;
      for(uint8_t x = 0; x < X_HBL_COUNT; ++x) {
        for(uint8_t y = 0; y < Y_HBL_COUNT; ++y) {
          if(temp_bed.enabled_mask & (1 << advanced_modular_bed->idx(x, y))) {
            sum += advanced_modular_bed->get_temp(x, y);
            count++;
          }
        }
      }
      temp_bed.celsius = sum / count;
}
#endif

#if HAS_TEMP_HEATBREAK_CONTROL
void Temperature::setTargetHeatbreak(const int16_t celsius, const uint8_t E_NAME) {
  temp_heatbreak[HOTEND_INDEX].target =
    #ifdef HEATBREAK_MAXTEMP
      _MIN(celsius, HEATBREAK_MAXTEMP)
    #else
      celsius
    #endif
  ;
  #if HAS_TOOLCHANGER()
    prusa_toolchanger.getTool(HOTEND_INDEX).set_heatbreak_target_temp(celsius);
  #endif
  start_watching_heatbreak(HOTEND_INDEX);
}
#endif

#if ENABLED(PIDTEMP)
void Temperature::updatePID() {
  #if ENABLED(PID_EXTRUSION_SCALING)
    last_e_position = 0;
  #endif
  #if HAS_TOOLCHANGER()
    // Set PID parameters to all dwarves
    for (auto tool : PhysicalToolIndex::all()) {
      const auto& pid = Temperature::temp_hotend[tool].pid;
      buddy::puppies::dwarfs[tool].set_pid(pid.Kp, pid.Ki, pid.Kd);
    }
  #endif
}
#endif
