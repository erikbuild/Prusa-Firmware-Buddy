/// @file
#pragma once

#include <span>
#include <atomic>

#include "base_hotend.hpp"

#include <module/temperature/marlin_temptable.hpp>

#if ENABLED(MODEL_DETECT_STUCK_THERMISTOR)
    #include <module/temperature/thermal_model_protection.hpp>
#endif

#if ENABLED(MODEL_BASED_HOTEND_REGULATOR)
    #include <module/temperature/hotend_regulator/model_based_hotend_regulator.hpp>
using HotendRegulator = ModelBasedHotendRegulator;

#else
    #include <module/temperature/hotend_regulator/standard_hotend_regulator.hpp>
using HotendRegulator = StandardHotendRegulator;
#endif

/// Represents a hotend that is controlled on the current processor (not on a dwarf)
class LocalHotend final : public BaseHotend {

public:
    using TempTable = std::span<const short[2]>;

    struct Config {
        BaseHotend::Config base_config;

        /// Temperature table for mapping raw temperature readouts
        TempTable nozzle_temp_table;

        /// "Marlin pin" controlling the nozzle heater
        /// Gets then passed through analogWrite/digitalWrite from Arduino.h,
        /// Which gets eventually mapped to the actual HAL calls in hwio_XX.cpp
        /// One day, it would be nice to untangle this mess.
        uint32_t nozzle_heater_marlin_pin;

#if ENABLED(HAS_HOTEND_AUTO_FAN)
        /// "Marlin pin" for controlling the autofan
        /// Autofan is a fan that is tied to the nozzle temperature (on if temp > EXTRUDER_AUTO_FAN_TEMPERATURE)
        uint32_t auto_fan_pin;
#endif

        /// Whether the nozzle heater should use software bitbanged PWM
        /// If true, the pin is actually controlled by digitalWrite() in Temperature::isr
        /// Otherwise, analogWrite() is used
        bool nozzle_heater_soft_pwm : 1;
    };

public:
    /// !!! Careful, the config pointer is stored, so make sure the config is persistent!
    explicit LocalHotend(PhysicalToolIndex tool, const Config *config);

    void set_nozzle_target_temp(TargetTemperature set) override;

protected:
    virtual void manage() override;

    virtual void isr_on_readings_ready() override;

    virtual void isr_soft_pwm(PWM255 phase) override;

protected:
    const Config &local_config_;

    MarlinTemptableRawMinMax nozzle_raw_temp_range_;

    HotendRegulator nozzle_regulator_;

#if ENABLED(MODEL_DETECT_STUCK_THERMISTOR)
    ThermalModelProtection thermal_model_protection_;
#endif

#if ENABLED(PID_EXTRUSION_SCALING)
    uint32_t last_e_position_ = 0;
#endif

    /// Written from the Temperature ISR, read from the defaultTask
    /// !!! Contains a sum of OVERSAMPLENR samples
    std::atomic<uint16_t> nozzle_raw_temp_;

#if ENABLED(HAS_HOTEND_AUTO_FAN)
    bool auto_fan_out_ : 1 = false;
#endif
};
