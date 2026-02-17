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

/**
 * temperature.h - temperature controller
 */

#include "thermistor/thermistors.h"

#include "../inc/MarlinConfig.h"

#if ENABLED(AUTO_POWER_CONTROL)
  #include "../feature/power.h"
#endif

#include <option/has_local_bed.h>
#include <option/has_modular_bed.h>
#if HAS_MODULAR_BED()
  #include "modular_heatbed.h"
#endif

#include <atomic>
#include <tool_index.hpp>
#include <utils/storage/strong_index_array.hpp>
#include <module/temperature/temp_defines.hpp>
#include <module/temperature/hotend_regulator/hotend_regulator.hpp>
#include <module/temperature/thermal_runaway.hpp>
#include <hotend/hotend.hpp>

#if HAS_PID_HEATING
  #define PID_K2           (1 - float(PID_K1))
  #define HEATBREAK_PID_K2 (1-float(HEATBREAK_PID_K1))
#endif

// A temperature sensor
typedef struct TempInfo {
  static constexpr float celsius_uninitialized = -1.0f;

  uint16_t acc;
  int16_t raw;
  float celsius = celsius_uninitialized;
  inline void reset() { acc = 0; }
  inline void sample(const uint16_t s) { acc += s; }
  inline void update() { raw = acc; }
} temp_info_t;

// A PWM heater with temperature sensor
typedef struct HeaterInfo : public TempInfo {
  int16_t target;
  uint8_t soft_pwm_amount;
} heater_info_t;

// A heater with PID stabilization
template<typename T>
struct PIDHeaterInfo : public HeaterInfo {
  T pid;  // Initialized by settings.load()
};

// Modular heater
#if HAS_MODULAR_BED()
struct ModularBedHeater: public HeaterInfo {
  uint16_t enabled_mask = 0xffff;
};
#endif

struct hotend_info_t {
  uint16_t acc;
  inline void sample(const uint16_t s) { acc += s; }
};

#if HAS_HEATED_BED
  #if ENABLED(PIDTEMPBED)
    typedef struct PIDHeaterInfo<PID_t> bed_info_t;
  #elif HAS_MODULAR_BED()
    typedef ModularBedHeater bed_info_t;
  #else
    typedef heater_info_t bed_info_t;
  #endif
#endif

#if HAS_TEMP_HEATBREAK
  #if ENABLED(PIDTEMPHEATBREAK)
    typedef struct PIDHeaterInfo<PID_t> heatbreak_info_t;
  #elif HAS_TEMP_HEATBREAK_CONTROL
    typedef heater_info_t heatbreak_info_t;
  #else
    typedef temp_info_t heatbreak_info_t;
  #endif

#endif

#if HAS_TEMP_BOARD
  typedef temp_info_t board_info_t;
#endif

#define THERMISTOR_ADC_RESOLUTION       1024           // 10-bit ADC .. shame to waste 12-bits of resolution on 32-bit
#define THERMISTOR_ABS_ZERO_C           -273.15f       // bbbbrrrrr cold !
#define THERMISTOR_RESISTANCE_NOMINAL_C 25.0f          // mmmmm comfortable

class Temperature {
  friend class MarlinTemptableRawMinMax;
  friend class BaseHotend;
  friend class LocalHotend;

  public:

    static volatile bool in_temp_isr;

    // we keep old array size instead of PhysicalToolIndex::count because of weak indexing (see definition of PhysicalToolIndex::count)
    static StrongIndexArray<hotend_info_t, HOTENDS, PhysicalToolIndex, PhysicalToolIndex::to_raw_static, strong_index_array::AllowWeakIndexing::yes> temp_hotend;

    #if HAS_HEATED_BED
      static bed_info_t temp_bed;
      // Estimated temperature of the bed frame as a rate-limited (linear)
      // value that converges to the real bed temperature at a slow rate.
      // Emulates heat propagation from the bed to the frame.
      static float bed_frame_est_celsius;
      static uint32_t bed_frame_millis;
    #endif

    #if HAS_TEMP_BOARD
      static board_info_t temp_board;
    #endif

    #if HAS_TEMP_HEATBREAK
      // we keep old array size instead of PhysicalToolIndex::count because of weak indexing (see definition of PhysicalToolIndex::count)
      static StrongIndexArray<heatbreak_info_t, HOTENDS, PhysicalToolIndex, PhysicalToolIndex::to_raw_static, strong_index_array::AllowWeakIndexing::yes> temp_heatbreak;
    #endif

    #if PRINTER_IS_PRUSA_iX()
      static TempInfo temp_psu;
      static TempInfo temp_ambient;
    #endif

    // For metrics only
    #if HAS_LOCAL_BED()
      std::atomic<int> bed_pwm;
    #endif
    std::atomic<int> nozzle_pwm;

    #if ENABLED(PREVENT_COLD_EXTRUSION)
      static bool allow_cold_extrude;
      static int16_t extrude_min_temp;
      FORCE_INLINE static bool tooCold(const int16_t temp) { return allow_cold_extrude ? false : temp < extrude_min_temp; }

      [[deprecated("Use the ToolIndex overload")]]
      FORCE_INLINE static bool tooColdToExtrude(const uint8_t E_NAME) {
        return tooCold(static_cast<int16_t>(degHotend(HOTEND_INDEX)));
      }
    #else
      [[deprecated("Use the ToolIndex overload")]]
      FORCE_INLINE static bool tooColdToExtrude(const uint8_t) { return false; }
    #endif

    inline static bool tooColdToExtrude(PhysicalToolIndex physical_tool) {
      return tooColdToExtrude(physical_tool.to_raw());
    }

    #if ENABLED(PID_EXTRUSION_SCALING)
      FORCE_INLINE static bool getExtrusionScalingEnabled() { return extrusion_scaling_enabled; }
      FORCE_INLINE static void setExtrusionScalingEnabled(bool enabled) { extrusion_scaling_enabled = enabled; }
    #endif

    #if ENABLED(PID_EXTRUSION_SCALING)
      static bool extrusion_scaling_enabled;
    #endif

  private:

    #if EARLY_WATCHDOG
      static bool inited;   // If temperature controller is running
    #endif

    static volatile bool temp_meas_ready;

    #if HAS_HEATED_BED
      #if DISABLED(PIDTEMPBED)
        static millis_t next_bed_check_ms;
      #endif
    #endif

    #if HAS_TEMP_HEATBREAK
      static millis_t next_heatbreak_check_ms;
    #endif

    #if HAS_AUTO_FAN
      static millis_t next_auto_fan_check_ms;
    #endif

  public:
    #if ENABLED(PID_EXTRUSION_SCALING)
      static int16_t lpq_len;
    #endif

    /**
     * Instance Methods
     */

    void init();

    /**
     * Static (class) methods
     */

    #if HAS_HEATED_BED
      static float analog_to_celsius_bed(const int raw);
    #endif
    #if HAS_TEMP_BOARD
      static float analog_to_celsius_board(const int raw);
    #endif

    #if HAS_TEMP_HEATBREAK
      static float analog_to_celsius_heatbreak(const int raw);
    #endif

    #if FAN_COUNT > 0

      static uint8_t fan_speed[FAN_COUNT]; ///< Configured fan speed
      static uint8_t applied_fan_speed[FAN_COUNT]; ///< Actually applied (and scaled) fan speed
      /// @note applyScaledFanSpeed() is used to scale and apply the speed from fan_speed to applied_fan_speed.

      #define FANS_LOOP(I) LOOP_L_N(I, FAN_COUNT)

      static uint16_t get_fan_speed(const uint8_t target);

      /**
       * @brief Scale and apply fan speeds to the fans.
       */
      static inline void applyScaledFanSpeed() {
        #if FAN_COUNT > 0
          FANS_LOOP(i) applied_fan_speed[i] = scaledFanSpeed(i);
        #endif
      }

      /**
       * @brief Scale and apply fan speeds to the fans.
       * This is used with fan speeds sampled from fan_speed by planner and delayed to match planner block processing.
       * @param delayed_fan_speed fan speeds to scale and apply
       */
      static inline void applyScaledFanSpeed(const uint8_t delayed_fan_speed[FAN_COUNT]) {
        #if FAN_COUNT > 0
          FANS_LOOP(i) applied_fan_speed[i] = scaledFanSpeed(i, delayed_fan_speed[i]);
        #endif
      }

      static void set_fan_speed(const uint8_t target, const uint16_t speed);
      static constexpr inline uint8_t fanPercent(const uint8_t speed) { return ui8_to_percent(speed); }

      static inline uint8_t scaledFanSpeed([[maybe_unused]] const uint8_t target, const uint8_t fs) {
        return (fs * uint16_t(
            128
        )) >> 7;
      }

      static inline uint8_t scaledFanSpeed(const uint8_t target) {
        return scaledFanSpeed(target, fan_speed[target]);
      }

    #endif // FAN_COUNT > 0

    static inline void zero_fan_speeds() {
      #if FAN_COUNT > 0
        FANS_LOOP(i) set_fan_speed(i, 0);
      #endif
    }

    /**
     * Called from the Temperature ISR
     */
    static void readings_ready();
    static void isr();

    /**
     * Call periodically to manage heaters
     */
    static void manage_heater() __O2; // __O2 added to work around a compiler error
    static inline void task() { manage_heater(); } // stub

    // Return true if the temperatures have been sampled at least once
    static bool temperatures_ready();

    /// @returns whether all the hotends and the bed have stabilized on the target temperature (or if the target temp is 0)
    static bool are_all_temperatures_reached();

    //high level conversion routines, for use outside of temperature.cpp
    //inline so that there is no performance decrease.
    //deg=degreeCelsius

    [[deprecated("Use the ToolIndex overload")]]
    FORCE_INLINE static float degHotend(const uint8_t E_NAME) {
      return Hotend::for_tool(HOTEND_INDEX).nozzle_temp();
    }

    inline static float degHotend(PhysicalToolIndex tool) {
      return Hotend::for_tool(tool).nozzle_temp();
    }

    [[deprecated("Use the Hotend functions directly")]]
    FORCE_INLINE static int16_t degTargetHotend(const uint8_t E_NAME) {
      return Hotend::for_tool(HOTEND_INDEX).nozzle_target_temp();
    }

    [[deprecated("Use the Hotend functions directly")]]
    inline static auto degTargetHotend(PhysicalToolIndex tool) {
      return Hotend::for_tool(tool).nozzle_target_temp();
    }

    inline static bool targetTooColdToExtrude(PhysicalToolIndex physical_tool) {
      #if ENABLED(PREVENT_COLD_EXTRUSION)
        return tooCold(degTargetHotend(physical_tool));
      #else
        return false;
      #endif
    }

    #if HAS_TEMP_HOTEND
      [[deprecated("Use the ToolIndex overload")]]
      static void setTargetHotend(const int16_t celsius, const uint8_t E_NAME);

      static inline void setTargetHotend(const int16_t celsius, PhysicalToolIndex tool) {
        setTargetHotend(celsius, tool.to_raw());
      }

      static bool are_hotend_temperatures_reached();

      static bool wait_for_hotend(const uint8_t target_extruder, const bool no_wait_for_cooling=true, bool fan_cooling=false);
    #endif

    #if HAS_HEATED_BED

      FORCE_INLINE static float degBed()          { return temp_bed.celsius; }
      FORCE_INLINE static int16_t degTargetBed()  { return temp_bed.target; }
      FORCE_INLINE static bool isHeatingBed()     { return temp_bed.target > temp_bed.celsius; }
      FORCE_INLINE static bool isCoolingBed()     { return temp_bed.target < temp_bed.celsius; }

      #if HAS_MODULAR_BED()
        FORCE_INLINE static uint16_t getEnabledBedletMask() {
          return temp_bed.enabled_mask;
        }
        FORCE_INLINE static void setEnabledBedletMask(const uint16_t enabled_mask) {
          if (temp_bed.enabled_mask != enabled_mask) {
            // When changing enabled bedlets, reset the estimated frame
            // temperature, so that it gets re-initialized to a fraction of the
            // current temp and gives some time for the frame temperature to
            // adjust to a different layout of the heat source.
            init_bed_frame_est_celsius();
          }

          temp_bed.enabled_mask = enabled_mask;
          for(uint8_t x = 0; x < X_HBL_COUNT; ++x) {
            for(uint8_t y = 0; y < Y_HBL_COUNT; ++y) {
              int16_t target_temp = 0;
              if(temp_bed.enabled_mask & (1 << advanced_modular_bed->idx(x, y))) {
                target_temp = temp_bed.target;
              }
              advanced_modular_bed->set_target(x, y, target_temp);
            }
          }
          advanced_modular_bed->update_bedlet_temps(temp_bed.enabled_mask, temp_bed.target);
          updateModularBedTemperature(); // update current temperature of modular bed - it will be now calculated from different bedlets
        }
        static void updateModularBedTemperature(); // will update temp_bed.celsius based on currently enabled bedlets

      #endif

      static void setTargetBed(const int16_t celsius);

      /// @returns whether the bed has stabilized on the target temperature (or if the target temp is 0)
      static bool is_bed_temperature_reached();

      static bool wait_for_bed(const bool no_wait_for_cooling=true);

      static void init_bed_frame_est_celsius();
      static void wait_for_frame_heatup();

    #endif // HAS_HEATED_BED

    #if HAS_TEMP_HEATBREAK
      [[deprecated("Use the ToolIndex overload")]]
      FORCE_INLINE static float degHeatbreak(const uint8_t E_NAME)            { return temp_heatbreak[HOTEND_INDEX].celsius; }
      
      inline static float degHeatbreak(PhysicalToolIndex tool) {
        return degHeatbreak(tool.to_raw());
      }

      #if HAS_TEMP_HEATBREAK_CONTROL
        [[deprecated("Use the ToolIndex overload")]]
        FORCE_INLINE static int16_t degTargetHeatbreak(const uint8_t E_NAME)  { return temp_heatbreak[HOTEND_INDEX].target; }

        inline static int16_t degTargetHeatbreak(PhysicalToolIndex tool) {
          return degTargetHeatbreak(tool.to_raw());
        }

        FORCE_INLINE static bool isHeatingHeatbreak(const uint8_t E_NAME)     { return temp_heatbreak[HOTEND_INDEX].target > temp_heatbreak[HOTEND_INDEX].celsius; }
        FORCE_INLINE static bool isCoolingHeatbreak(const uint8_t E_NAME)     { return temp_heatbreak[HOTEND_INDEX].target < temp_heatbreak[HOTEND_INDEX].celsius; }

        static void suspend_heatbreak_fan(millis_t ms);
      #endif
    #endif // HAS_TEMP_HEATBREAK

    #if HAS_TEMP_HEATBREAK_CONTROL
      [[deprecated("Use the ToolIndex overload")]]
      static void setTargetHeatbreak(const int16_t celsius, const uint8_t E_NAME);

      inline static void setTargetHeatbreak(int16_t celsius, PhysicalToolIndex tool) {
        setTargetHeatbreak(celsius, tool.to_raw());
      }
    #endif // HAS_TEMP_HEATBREAK

    #if HAS_TEMP_BOARD
      FORCE_INLINE static float degBoard()            { return temp_board.celsius; }
    #endif // HAS_TEMP_BOARD

    #if PRINTER_IS_PRUSA_iX()
      FORCE_INLINE static float deg_psu() { return temp_psu.celsius; }
      FORCE_INLINE static float deg_ambient() { return temp_ambient.celsius; }
    #endif

    /**
     * The software PWM power for a heater
     */
    static int16_t getHeaterPower(const heater_ind_t heater);

private:
    enum class disable_bed_t : bool {no, yes};
    /**
     * used by disable_all_heaters and disable_hotend
     */
    static void disable_heaters(disable_bed_t disable_bed);
    
public:
    /**
     * Switch off all heaters, set all target temperatures to 0
     */
    static void disable_all_heaters();
    /**
     * Like above, but disables only heaters on local CPU.
     *
     * The ones run by a separate CPU is left intact. Can be used in
     * interrupts, as this avoids interprocessor communication.
     */
    static void disable_local_heaters();
    /**
     * Switch off all hotends, set all hotend target temperatures to 0
     */
    static void disable_hotend();

    #if HAS_TEMP_SENSOR
      static void print_heater_states(const uint8_t target_extruder);
      #if ENABLED(AUTO_REPORT_TEMPERATURES)
        static uint8_t auto_report_temp_interval;
        static millis_t next_temp_report_ms;
        static void auto_report_temperatures();
        static inline void set_auto_report_interval(uint8_t v) {
          NOMORE(v, 60);
          auto_report_temp_interval = v;
          next_temp_report_ms = millis() + 1000UL * v;
        }
      #endif
    #endif
    
    static void _temp_error(const heater_ind_t e, PGM_P const serial_msg, PGM_P const lcd_msg);
    static void min_temp_error(const heater_ind_t e);
    static void max_temp_error(const heater_ind_t e);

  private:
    static void set_current_temp_raw();
    static void updateTemperaturesFromRawValues();

    static void checkExtruderAutoFans();

    #if ENABLED(PIDTEMPBED)
      static float get_pid_output_bed();
    #endif

    #define HAS_THERMAL_PROTECTION (ENABLED(THERMAL_PROTECTION_HOTENDS) || HAS_THERMALLY_PROTECTED_BED)

    #if HAS_THERMAL_PROTECTION
      #if HAS_THERMALLY_PROTECTED_BED
        static ThermalRunaway thermal_runaway_bed;
      #endif
    #endif // HAS_THERMAL_PROTECTION
};

extern Temperature thermalManager;
