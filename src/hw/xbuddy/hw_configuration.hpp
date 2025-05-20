/// @file
#pragma once

#include "hw_configuration_common.hpp"
#include <device/board.h>

static_assert(BOARD_IS_XBUDDY());

namespace buddy::hw {

/// Abstracts runtime dynamic configuration based on incompatibilities
/// between different hardware revisions.
class Configuration : public ConfigurationCommon {
    Configuration();
    Configuration(const Configuration &) = delete;

    uint8_t loveboard_bom_id;
    bool loveboard_present;

public:
    static Configuration &Instance();

    bool has_inverted_fans() const;
    bool has_inverted_mmu_reset() const;
    bool has_mmu_power_up_hw() const;
    bool has_trinamic_oscillators() const;
    bool is_fw_incompatible_with_hw() const;
    bool needs_heatbreak_thermistor_table_5() const;
    bool needs_push_pull_mmu_reset_pin() const;
    bool needs_software_mmu_powerup() const;
    float curr_measurement_voltage_to_current(float voltage) const;
};

} // namespace buddy::hw
