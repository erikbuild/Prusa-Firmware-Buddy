/// @file
#pragma once

#include <hotend/hotend.hpp>

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
    virtual void set_nozzle_target_temp(TargetTemperature set) override;

protected:
    /// !!! Careful, the config pointer is stored, so make sure the config is persistent!
    explicit BaseHotend(PhysicalToolIndex tool, const Config *config);

protected:
    const Config &base_config_;
    const PhysicalToolIndex tool_;
};
