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

#if HAS_SIDE_FSENSOR_INVERTIBLE()
void FilamentSensorCalibratorBasic::calibrate_polarity() {
    // The user has just confirmed no filament. If the sensor disagrees, the magnet polarity is reversed — flip it.
    if (sensor_.get_state() == FilamentSensorState::HasFilament) {
        sensor_.set_polarity_inverted(!sensor_.is_polarity_inverted());
    }
}
#endif

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
    const auto all_correct = [](const Stats &s) { return s.total > 0 && s.correct == s.total; };

#if HAS_SIDE_FSENSOR_INVERTIBLE()
    const auto all_opposite = [](const Stats &s) { return s.total > 0 && s.opposite == s.total; };
    // Both phases consistently inverted -> magnet polarity got swapped between calibrate_polarity() and now. Persist the flip.
    if (all_opposite(not_inserted_) && all_opposite(inserted_)) {
        sensor_.set_polarity_inverted(!sensor_.is_polarity_inverted());
        return;
    }
#endif

    fail_if(!all_correct(not_inserted_));
    fail_if(!all_correct(inserted_));
}
