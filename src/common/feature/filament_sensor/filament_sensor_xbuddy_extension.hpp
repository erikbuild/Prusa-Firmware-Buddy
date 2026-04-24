#pragma once

#include <feature/filament_sensor/filament_sensor.hpp>
#include <xbuddy_extension/shared_enums.hpp>

class FSensorXBuddyExtension : public IFSensor {

public:
    enum class Source {
        /// Single fsensor on the GPIO connector
        gpio,

        /// OneWire-based array of sensors on the EXT connector
        ext,
    };

    FSensorXBuddyExtension(FilamentSensorID id, Source source);

    bool is_calibrated() const override;
    TestResult get_selftest_result() const override;

protected:
    virtual void cycle() override;
    virtual int32_t GetFilteredValue() const override;

private:
    FilamentSensorState interpret_state() const;

    const Source source_;

    /// Raw hardware state from XBuddy Extension, exposed via GetFilteredValue() for debugging
    mutable xbuddy_extension::FilamentSensorState raw_state_ = xbuddy_extension::FilamentSensorState::uninitialized;
};
