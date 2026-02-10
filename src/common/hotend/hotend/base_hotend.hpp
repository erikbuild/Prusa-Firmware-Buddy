/// @file
#pragma once

#include <hotend/hotend.hpp>

/// Represents a base for all non-dummy hotends
class BaseHotend : public Hotend {

public:
    virtual void set_nozzle_target_temp(TargetTemperature set) override;

protected:
    explicit BaseHotend(PhysicalToolIndex tool);

protected:
    const PhysicalToolIndex tool_;
};
