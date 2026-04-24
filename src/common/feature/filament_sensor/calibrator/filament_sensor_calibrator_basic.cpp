#include "filament_sensor_calibrator_basic.hpp"

#include <logging/log.hpp>

LOG_COMPONENT_REF(FSensor);

namespace {

enum class Reading {
    correct,
    opposite,
    indeterminate,
};

Reading classify(FilamentSensorState state, FilamentSensorCalibrator::CalibrationPhase phase) {
    using CalibrationPhase = FilamentSensorCalibrator::CalibrationPhase;
    switch (phase) {
    case CalibrationPhase::not_inserted:
        if (state == FilamentSensorState::NoFilament) {
            return Reading::correct;
        }
        if (state == FilamentSensorState::HasFilament) {
            return Reading::opposite;
        }
        return Reading::indeterminate;
    case CalibrationPhase::inserted:
        if (state == FilamentSensorState::HasFilament) {
            return Reading::correct;
        }
        if (state == FilamentSensorState::NoFilament) {
            return Reading::opposite;
        }
        return Reading::indeterminate;
    case CalibrationPhase::_cnt:
        break;
    }
    bsod_unreachable();
}

} // namespace

FilamentSensorCalibratorBasic::FilamentSensorCalibratorBasic(IFSensor &sensor)
    : FilamentSensorCalibrator(sensor) {}

bool FilamentSensorCalibratorBasic::is_ready_for_calibration(CalibrationPhase phase) const {
    return classify(sensor_.get_state(), phase) == Reading::correct;
}

void FilamentSensorCalibratorBasic::calibrate(CalibrationPhase phase) {
    Stats *stats = nullptr;
    switch (phase) {
    case CalibrationPhase::not_inserted:
        stats = &not_inserted_;
        break;
    case CalibrationPhase::inserted:
        stats = &inserted_;
        break;
    case CalibrationPhase::_cnt:
        bsod_unreachable();
    }

    ++stats->total;
    const auto reading = classify(sensor_.get_state(), phase);
    if (reading == Reading::correct) {
        ++stats->correct;
    } else if (reading == Reading::opposite) {
        ++stats->opposite;
    }
}

void FilamentSensorCalibratorBasic::finish() {
    // We require 75% of samples to be correct and 25% can be opposite (wiggle in the filament sensor)
    const auto mostly_correct = [](const Stats &s) { return s.total > 0 && s.correct * 4 >= s.total * 3; };
    const auto few_opposite = [](const Stats &s) { return s.opposite * 4 <= s.total; };
    fail_if(!mostly_correct(not_inserted_));
    fail_if(!mostly_correct(inserted_));
    fail_if(!few_opposite(not_inserted_));
    fail_if(!few_opposite(inserted_));
}
