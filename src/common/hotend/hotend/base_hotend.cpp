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
    t.temp_hotend_residency_start_ms[tool_] = 0;

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
}
