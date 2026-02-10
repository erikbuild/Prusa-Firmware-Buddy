/// @file
#include "base_hotend.hpp"

#include <module/temperature.h>
#include <module/temperature/temperature_declares.hpp>

#include <option/board_is_master_board.h>
#if BOARD_IS_MASTER_BOARD()
    #include <marlin_server.hpp>
    #include <feature/safety_timer/safety_timer.hpp>
#endif

BaseHotend::BaseHotend(PhysicalToolIndex tool, const Config *config)
    : base_config_(*config)
    , tool_(tool) {
    auto &r = temp_range[tool_];

    // TODO: Move the whole structure here
    // It's a mess, will do in a next PR
    r.mintemp = base_config_.min_nozzle_temp;
    r.maxtemp = base_config_.max_nozzle_temp;
}

void BaseHotend::set_nozzle_target_temp(TargetTemperature set) {
#if BOARD_IS_MASTER_BOARD()
    // We cannot overwrite target temps while the safety_timer is active, deactivate it first
    buddy::safety_timer().reset_restore_nonblocking();
#endif

    auto &t = thermalManager;

    const int16_t new_temp = std::min<int16_t>(set, base_config_.max_nozzle_temp - HEATER_MAXTEMP_SAFETY_MARGIN);

#if ENABLED(AUTO_POWER_CONTROL)
    if (set) {
        powerManager.power_on();
    }
#endif

    if (nozzle_target_temp_ == new_temp) {
        return;
    }

    // target changed, reset time when it reached target
    nozzle_temp_residency_start_ms_ = 0;

    nozzle_target_temp_ = new_temp;

#if BOARD_IS_MASTER_BOARD()
    // This is a legit use
    marlin_server::call_manually::set_temp_to_display(new_temp, tool_.to_raw());
#endif

#if WATCH_HOTENDS
    watch_hotend[tool_].reset(t.degHotend(tool_), new_temp);
#endif
}

void BaseHotend::manage() {
    if (nozzle_temp() > base_config_.max_nozzle_temp) {
        thermalManager.max_temp_error((heater_ind_t)tool_.to_raw());
    }

    if ((nozzle_target_temp() > 0) && (nozzle_temp() < base_config_.min_nozzle_temp)) {
        thermalManager.min_temp_error((heater_ind_t)tool_.to_raw());
    }

    manage_temp_residency();

#if ENABLED(THERMAL_PROTECTION_HOTENDS)
    // Note: This being in BaseHotend means that we're checking for this twice on remote hotends
    // But better safe than sorry
    thermal_runaway_.step(nozzle_temp(), nozzle_target_temp(), (heater_ind_t)tool_.to_raw(), THERMAL_PROTECTION_PERIOD, THERMAL_PROTECTION_HYSTERESIS);
#endif
}

void BaseHotend::manage_temp_residency() {
    const auto now = millis();
    const auto temp_diff = std::abs(nozzle_target_temp() - nozzle_temp());

    if (!nozzle_temp_residency_start_ms_ && temp_diff < TEMP_WINDOW) {
        nozzle_temp_residency_start_ms_ = now;

    } else if (temp_diff > TEMP_HYSTERESIS) {
        nozzle_temp_residency_start_ms_ = 0;
    }

    nozzle_temp_reached_ = //
        (nozzle_target_temp() <= 0) //
        || ( //
            nozzle_temp_residency_start_ms_ //
            && !PENDING(now, nozzle_temp_residency_start_ms_ + (TEMP_RESIDENCY_TIME)*1000UL) //
        );
}
