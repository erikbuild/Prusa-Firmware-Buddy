/// @file
#pragma once

#include <hotend/hotend.hpp>

/// Represents a hotend that does nothing at all
/// Used for NoTool hotends
class DummyHotend final : public Hotend {

public:
    explicit DummyHotend();

    void set_nozzle_target_temp([[maybe_unused]] TargetTemperature set) override {}

protected:
    virtual void manage() override {}
};
