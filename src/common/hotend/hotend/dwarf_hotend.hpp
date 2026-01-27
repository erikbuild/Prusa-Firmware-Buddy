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
    virtual void set_nozzle_target_temp(TargetTemperature set) override;

protected:
    virtual void manage() override;
};
