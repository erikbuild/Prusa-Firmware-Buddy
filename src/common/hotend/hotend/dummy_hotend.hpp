/// @file
#pragma once

#include <hotend/hotend.hpp>

/// Represents a hotend that does nothing at all
/// Used for NoTool hotends
class DummyHotend final : public Hotend {

public:
    explicit DummyHotend();

    void set_nozzle_target_temp([[maybe_unused]] TargetTemperature set) override {}

    const HotendPIDConfig &nozzle_pid_config() const override {
        static constexpr HotendPIDConfig dummy;
        return dummy;
    }

    void set_nozzle_pid_config([[maybe_unused]] const HotendPIDConfig &set) override {}

protected:
    virtual void manage() override {}
};
