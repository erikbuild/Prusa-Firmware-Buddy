/// @file
#pragma once

#include <hotend/hotend.hpp>
#include <module/temperature/thermal_runaway.hpp>
#include <module/temperature/heater_watch.hpp>

/// Represents a base for all non-dummy hotends
class BaseHotend : public Hotend {

public:
    struct Config {
        /// Minimum acceptable temperature for the hotend
        /// Exceeding this limit results in a RSOD
        /// Formerly done by the HEATER_0_MINTEMP macro
        TargetTemperature min_nozzle_temp;

        /// Maximum acceptable temperature for the hotend
        /// Exceeding this limit results in a RSOD
        /// Formerly done by the HEATER_0_MAXTEMP macro
        TargetTemperature max_nozzle_temp;
    };

public:
    void set_nozzle_target_temp(TargetTemperature set) override;

#if HAS_TEMP_HEATBREAK_CONTROL
    void set_heatbreak_target_temp(TargetTemperature set) override;
#endif

protected:
    /// !!! Careful, the config pointer is stored, so make sure the config is persistent!
    explicit BaseHotend(PhysicalToolIndex tool, const Config *config);

    // !!! MUST be called after temps are set properly
    // Note: the = 0; is here to enforce overriding.
    // !!! The function is actually implemented and MUST be called from the overriding function.
    virtual void manage() override = 0;

    void manage_temp_residency();

protected:
    const Config &base_config_;
    const PhysicalToolIndex tool_;

#if ENABLED(THERMAL_PROTECTION_HOTENDS)
    ThermalRunaway thermal_runaway_;
#endif

#if WATCH_HOTENDS
    HeaterWatch heater_watch_;
#endif

    /// timestamp when temeperature reached target +-TEMP_WINDOW, 0 when outside this window
    /// note: 0 is valid timestamp, but if temperature reaches window at time 0, it will just be evaluated again little later, so it doesn't cause any bug
    uint32_t nozzle_temp_residency_start_ms_ = 0;
};
