/// @file
#include "hw_configuration.hpp"

#include "data_exchange.hpp"
#include "otp.hpp"
#include "timing_precise.hpp"
#include <option/bootloader.h>
#include <option/has_door_sensor.h>
#if HAS_DOOR_SENSOR()
    #include <buddy/door_sensor.hpp>
#endif

/*
+-----++-------------------------------------------------------------------------------+-------------------------+-------------------+
|pin  || MK4 xBuddy027                                                                 | MK4 xBuddy037           | MK3.5 xBuddy037   |
+-----++-------------------------------------------------------------------------------+-------------------------+-------------------+
|PA6  || Heatbreak temp                                                                | Heatbreak temp          | FILAMENT_SENSOR   |
|PA10 || accel CS                                                                      | accel CS                | TACHO0            |
|PE9  || FAN1 PWM                                                                      | FAN1 PWM inverted       | FAN1 PWM inverted |
|PE11 || FAN0 PWM                                                                      | FAN0 PWM inverted       | FAN0 PWM inverted |
|PE10 || both fans tacho / fan 0 tacho (old loveboard)                                 | both fans tacho         | TACHO1            |
|PE14 || NOT CONNECTED (requires R191 0R)  / fan 1 tacho (old loveboard + requires D6) | EXTRUDER_SWITCH         | EXTRUDER_SWITCH   |
|PF5  || -                                                                             | -                       | PINDA_THERM       | .. IGNORE
|PF13 || eeprom, fan multiplexor                                                       | eeprom, fan multiplexor | ZMIN (PINDA)      |
+-----++-------------------------------------------------------------------------------+-------------------------+-------------------+

LOVEBOARD support
need at least v31 (with EXTRUDER_SWITCH == multiplexer on fan tacho pins), xBuddy must have R191 0R instead of D6 to support 3V3 reference for LOVEBOARD
following table contains only differences with loveboard >= v31 and without old PINDA support (only SuperPINDA suported for MK3 extruder)
+-----++----------------------------------------------+-------------------------------+-------------------+
|pin  || MK4 xBuddy027 (with R191 0R)                 | MK4 xBuddy037                 | MK3.5 xBuddy037   |
+-----++----------------------------------------------+-------------------------------+-------------------+
|PA6  || Heatbreak temp                               | Heatbreak temp                | FILAMENT_SENSOR   |
|PA10 || accel CS                                     | accel CS                      | TACHO0            |
|PE9  || FAN1 PWM inverted                            | FAN1 PWM                      | FAN1 PWM          |
|PE10 || both fans tacho                              | both fans tacho               | TACHO1            |
|PE11 || FAN0 PWM inverted                            | FAN0 PWM                      | FAN0 PWM          |
|PE14 || NOT CONNECTED == same as MK4 on 037 (pullup) | EXTRUDER_SWITCH .. use pullup | EXTRUDER_SWITCH   |
|PF13 || eeprom, fan multiplexor                      | eeprom, fan multiplexor       | ZMIN (PINDA)      |
+-----++----------------------------------------------+-------------------------------+-------------------+


PC0 HOTEND_NTC is the same for all versions, but needs EXTRUDER_SWITCH enabled to provide pullup for MK3.5

xBuddy037 FW related changes
disconnected power panic cable will cause power panic
current measurement changed from 5V to 3V3 - need to recalculate values
MMU switching changed - no need to generate pulses anymore
MMU_RESET logic inverted
*/

namespace buddy::hw {

Configuration &Configuration::Instance() {
    static Configuration ths = Configuration();
    return ths;
}

Configuration::Configuration() {
#if PRINTER_IS_PRUSA_MK3_5()
    auto bom_id = otp_get_bom_id();

    if (!bom_id || *bom_id == 27) {
        bsod("Wrong board version");
    }
#endif
    loveboard_bom_id = data_exchange::get_loveboard_eeprom().bomID;
    loveboard_present = data_exchange::get_loveboard_status().data_valid;
}

bool Configuration::has_inverted_fans() const {
    return get_board_bom_id() < 37;
}

bool Configuration::has_inverted_mmu_reset() const {
    return get_board_bom_id() >= 37;
}

bool Configuration::has_mmu_power_up_hw() const {
    return get_board_bom_id() >= 37;
}

bool Configuration::has_trinamic_oscillators() const {
    return get_board_bom_id() >= 37;
}

bool Configuration::is_fw_incompatible_with_hw() const {
#if PRINTER_IS_PRUSA_COREONE()
    if (buddy::door_sensor().detailed_state().state == buddy::DoorSensor::State::sensor_detached) {
        return true;
    }
    return false;
#elif PRINTER_IS_PRUSA_MK4()
    if (buddy::door_sensor().detailed_state().state != buddy::DoorSensor::State::sensor_detached) {
        return true;
    }

    if (loveboard_present) {
        return false; // valid data, fw compatible
    }

    // This procedure is checking if MK3.5 extruder is installed,
    // in which case we have an incompatible FW (MK4) and HW (MK3.5)
    // It is not possible to continue and info screen saying to reflash has to pop up
    const size_t count_of_validation_edges = 4;
    bool mk35_extruder_detected = true;
    for (size_t i = 0; i < count_of_validation_edges; ++i) {
        hx717Sck.write(Pin::State::low);
        delay_us_precise<1000>();
        if (hx717Dout.read() != Pin::State::low) {
            mk35_extruder_detected = false;
            break;
        }
        hx717Sck.write(Pin::State::high);
        delay_us_precise<1000>();
        if (hx717Dout.read() != Pin::State::high) {
            mk35_extruder_detected = false;
            break;
        }
    }
    return mk35_extruder_detected;
#elif PRINTER_IS_PRUSA_MK3_5()
    // valid data from loveboard means that we have MK4 HW, since MK3.5 does not have loveboard
    return loveboard_present;
#elif PRINTER_IS_PRUSA_iX()
    return false;
#else
    #error
#endif
}

float Configuration::curr_measurement_voltage_to_current(float voltage) const {
    // Allegro ACS711KEXLT-15AB
    // +-15 A, 90mV/A, 0A -> output == Vcc/2
    // result in mA
    //
    // XBuddy 0.3.4 3V3 reference
    // XBuddy < 0.3.4 5V reference

    constexpr float allegro_curr_from_voltage = 1 / 0.09F;

    const float allegro_zero_curr_voltage = (get_board_bom_id() == 27) ? 5.F / 2.F : 3.35F / 2.F; // choose half of 3V3 or 5V range

    return (voltage - allegro_zero_curr_voltage) * allegro_curr_from_voltage;
}

#if PRINTER_IS_PRUSA_MK3_5()
// Definition intentionally missing, MK3.5 has no heatbreak thermistor at all
// so we want to get linker error if any code actually calls this.
#else
bool Configuration::needs_heatbreak_thermistor_table_5() const {
    return (loveboard_bom_id < 33 && loveboard_bom_id != 0) // error -> expect more common variant
        || loveboard_bom_id == 0xff; // error when run in simulator -> simulator uses table 5
}
#endif

bool Configuration::needs_push_pull_mmu_reset_pin() const {
    // xBuddy schematics says: Revisions older than 34 must use open drain only.
    return get_board_bom_id() >= 34;
}

bool Configuration::needs_software_mmu_powerup() const {
    // TODO: When we have a new bom this should be edited
    return true;
}

} // namespace buddy::hw
