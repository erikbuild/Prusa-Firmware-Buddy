#pragma once

#include <cstdint>

namespace indx_nozzle_cleaner_calibration {

void run();

/// Data sent via PhaseData for evaluating phases (must fit in 4 bytes)
struct EvaluatingData {
    int16_t offset_hundredths; ///< Measured offset in 0.01 mm units
    int16_t nominal_tenths; ///< Nominal position in 0.1 mm units

    static EvaluatingData from(float offset, float nominal) {
        return {
            .offset_hundredths = static_cast<int16_t>(offset * 100.0f),
            .nominal_tenths = static_cast<int16_t>(nominal * 10.0f),
        };
    }

    float offset() const { return static_cast<float>(offset_hundredths) / 100.0f; }
    float nominal() const { return static_cast<float>(nominal_tenths) / 10.0f; }
};
static_assert(sizeof(EvaluatingData) <= 4);

} // namespace indx_nozzle_cleaner_calibration
