/// \file
#pragma once

#include <feature/filament_sensor/calibrator/filament_sensor_calibrator.hpp>
#include <feature/filament_sensor/filament_sensor.hpp>

class FilamentSensorCalibratorBasic final : public FilamentSensorCalibrator {

public:
    FilamentSensorCalibratorBasic(IFSensor &sensor);

    bool is_ready_for_calibration(CalibrationPhase phase) const final;
#if HAS_SIDE_FSENSOR_INVERTIBLE()
    void calibrate_polarity() final;
#endif
    void calibrate(CalibrationPhase phase) final;
    void finish() final;

private:
    struct Stats {
        uint16_t correct = 0;
        uint16_t opposite = 0;
        uint16_t total = 0;
    };
    Stats not_inserted_;
    Stats inserted_;
};
