#pragma once

#include <feature/filament_sensor/filament_sensor.hpp>

/// Base class for an invertible binary filament sensor
/// Polarity of the fsensor is determined during calibration
class FSensorInvertible : public IFSensor {

public:
    static FilamentSensorState inverted_state(FilamentSensorState state, bool inverted = true);

public:
    FSensorInvertible(FilamentSensorID id);

    FilamentSensorCalibrator *create_calibrator(FilamentSensorCalibrator::Storage &storage) override;

    /// Load filament sensor settings from the EEPROM
    void load_settings();

    bool is_calibrated() const final {
        return is_calibrated_;
    }

    FilamentSensorState raw_state() const {
        return raw_state_;
    }

    int32_t GetFilteredValue() const final {
        return static_cast<int32_t>(raw_state_.load());
    }

protected:
    /// Updates state from raw_state
    /// !!! To be overriden and called by the children AFTER raw_state is updated
    virtual void cycle() override = 0;

protected:
    std::atomic<bool> is_calibrated_ = false;
    std::atomic<bool> is_inverted_ = false;

    /// Raw filament sensor state before inversion
    /// To be updated in cycle() of the child
    std::atomic<FilamentSensorState> raw_state_ = FilamentSensorState::Disabled;

private:
    // Make the state after inversion private, we don't want children to write to it directly
    using IFSensor::state;
};
