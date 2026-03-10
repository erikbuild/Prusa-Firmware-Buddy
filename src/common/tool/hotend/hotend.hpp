/// @file
#pragma once

#include <tool_index.hpp>
#include <utils/uncopyable.hpp>
#include <utils/variant_utils.hpp>
#include <module/temperature/hotend_regulator/hotend_regulator.hpp>
#include <pwm_utils.hpp>
#include <atomic>

struct FilamentTypeParameters;

/// Class representing a hotend
/// This is an abstract class, hotend implementations differ
class Hotend : public Uncopyable {
    friend class Temperature;

public:
    /// in °C
    using Temperature = float;
    static constexpr Temperature temperature_uninitialized = -1;

    /// in °C
    /// <= 0 = no target temperature/invalid value
    using TargetTemperature = int16_t;

public:
    /// @returns Hotend of the tool
    /// There should be a 1:1 mapping
    /// Implemented differently for each printer in in hotends_XX.cpp
    /// !!! To be accessed only from the marlin task
    static Hotend &for_tool(PhysicalToolIndex tool);

    static Hotend &for_tool(std::variant<PhysicalToolIndex, NoTool> tool);

    [[deprecated("Use the strong typed variant")]]
    static Hotend &for_tool(uint8_t tool);

public:
    virtual bool supports_filament(const FilamentTypeParameters &filament) const = 0;

    /// Current temperature of the nozzle
    Temperature nozzle_temp() const {
        return nozzle_temp_;
    }

    /// @returns whether the nozzle temperature has stabilized on the target
    bool is_nozzle_temp_reached() const {
        return nozzle_temp_reached_;
    }

    /// Target temperature of the nozzle
    TargetTemperature nozzle_target_temp() const {
        return nozzle_target_temp_;
    }

    virtual void set_nozzle_target_temp(TargetTemperature set) = 0;

    const HotendPIDConfig &nozzle_pid_config() const {
        return nozzle_pid_config_;
    }

    virtual void set_nozzle_pid_config(const HotendPIDConfig &set) {
        nozzle_pid_config_ = set;
    }

    /// Compatibility function for heater selftests
    PID_t nozzle_pid_config_compat() const;

    /// Compatibility function for heater selftests
    void set_nozzle_pid_config_compat(const PID_t &set);

#if ENABLED(MODEL_DETECT_STUCK_THERMISTOR)
    bool is_thermal_model_protection_ok() const {
        return thermal_model_protection_ok_;
    }
#endif

    PWM255 nozzle_heater_pwm() const {
        return PWM255(nozzle_heater_pwm_);
    }

#if HAS_TEMP_HEATBREAK
    Temperature heatbreak_temp() const {
        return heatbreak_temp_;
    }
#endif

#if HAS_TEMP_HEATBREAK_CONTROL
    TargetTemperature heatbreak_target_temp() const {
        return heatbreak_target_temp_;
    }

    virtual void set_heatbreak_target_temp(TargetTemperature set) = 0;

    PWM255 heatbreak_fan_pwm() const {
        return heatbreak_fan_pwm_;
    }
#endif

protected:
    explicit Hotend() = default;

protected:
    /// This function is called from the DefaultTask at regular intervals (from temperature.manage_heater())
    virtual void manage() = 0;

    /// Raw values are accumulated by the Temperature::isr to temp_hotend.acc
    /// Once there's OVERSAMPLENR values accumulated, this function is called
    /// It is supposed to pass the accumulated values to the defaultTask
    /// And reset the accumulators
    virtual void isr_on_readings_ready() {};

    /// Called from TemperatureISR to control bitbanged PWMs, if the hotend needs it.
    /// @param phase deterines the current value of the soft PWM counter. Pins should output high if phase <= pin_pwm_target
    virtual void isr_soft_pwm(PWM255 phase) { (void)phase; }

protected:
    HotendPIDConfig nozzle_pid_config_;
    Temperature nozzle_temp_ = temperature_uninitialized;

#if HAS_TEMP_HEATBREAK
    Temperature heatbreak_temp_ = temperature_uninitialized;
#endif

    TargetTemperature nozzle_target_temp_ = 0;

#if HAS_TEMP_HEATBREAK_CONTROL
    TargetTemperature heatbreak_target_temp_ = 0;
#endif

    /// Output power of the nozzle heater
    /// For local hotends, this is set in Hotend::manage and potentially used for soft pwm control
    /// For remote hotends, this is retrieved from the remote board and only used for display/reporting purposes
    /// Possibly accessed from isr_soft_pwm, thus needs to be atomic
    std::atomic<uint8_t> nozzle_heater_pwm_ = 0;

#if HAS_TEMP_HEATBREAK
    PWM255 heatbreak_fan_pwm_;
#endif

    bool nozzle_temp_reached_ : 1 = false;

#if ENABLED(MODEL_DETECT_STUCK_THERMISTOR)
    bool thermal_model_protection_ok_ : 1 = false;
#endif
};
