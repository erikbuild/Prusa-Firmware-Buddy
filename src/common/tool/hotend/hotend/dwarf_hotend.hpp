/// @file
#pragma once

#include "base_hotend.hpp"

/// Represents a hotend that is managed by a dwarf on an XL
class DwarfHotend final : public BaseHotend {

public:
    /// !!! Careful, the config pointer is stored, so make sure the config is persistent!
    explicit DwarfHotend(PhysicalToolIndex tool, const Config *config)
        : BaseHotend(tool, config) {}

public:
    void set_nozzle_target_temp(TargetTemperature set) override;

#if HAS_TEMP_HEATBREAK_CONTROL
    void set_heatbreak_target_temp(TargetTemperature set) override;
#endif

    void set_nozzle_pid_config(const HotendPIDConfig &set) override;

protected:
    virtual void manage() override;
};
