/// \file
#pragma once

#include <array>
#include <feature/filament_sensor/calibrator/filament_sensor_calibrator.hpp>
#include <feature/filament_sensor/filament_sensor_invertible.hpp>

/// Polarity-discovery calibrator for binary side sensors that may have reversed magnet polarity.
/// Records the observed sensor state for each calibration phase and persists the polarity
/// inversion in finish() based on the observed direction of the transition.
class FilamentSensorCalibratorInvertible final : public FilamentSensorCalibrator {

public:
    explicit FilamentSensorCalibratorInvertible(FSensorInvertible &sensor);

public:
    bool is_ready_for_calibration(CalibrationPhase phase) const final;
    void calibrate(CalibrationPhase phase) final;
    void finish() final;

protected:
    static FilamentSensorState expected_state(CalibrationPhase phase, bool inverted);

private:
    FSensorInvertible &sensor_;

    /// Stores whether the fsensor is ok, assuming it is not inverted
    bool is_ok_not_inverted_ : 1 = true;

    /// Stores whether the fsensor is ok, assuming it is inverted
    bool is_ok_inverted_ : 1 = true;
};
