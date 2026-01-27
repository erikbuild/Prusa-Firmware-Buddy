/// @file
#include "local_hotend.hpp"

#include <module/thermistor/thermistors.h>
#include <module/temperature.h>
#include <module/temperature/temperature_declares.hpp>

LocalHotend::LocalHotend(PhysicalToolIndex tool, const Config *config)
    : BaseHotend(tool, &config->base_config)
    , local_config_(*config) {

    // Setup heater temp range
    temp_range[tool_] = temp_range_t(local_config_.nozzle_temp_table, base_config_.min_nozzle_temp, base_config_.max_nozzle_temp);
}

void LocalHotend::manage() {
    nozzle_temp_ = marlin_temptable_lookup(local_config_.nozzle_temp_table, nozzle_raw_temp_);

    // !!! MUST be called after temps are set properly
    BaseHotend::manage();
}

void LocalHotend::isr_on_readings_ready() {
    auto &th = thermalManager.temp_hotend[tool_.to_raw()];

    // Note: Before Hotend refactoring, updating the raw value was waiting for temp_meas_ready
    // Now, we are using std::atomic like sane people, so it shouldn't be necessary
    nozzle_raw_temp_ = th.acc;
    th.acc = 0;

    const bool heater_on = (nozzle_target_temp() > 0
#if ENABLED(PIDTEMP)
        || th.soft_pwm_amount > 0
#endif
    );

    temp_range[tool_].raw.check_temperror(nozzle_raw_temp_, (heater_ind_t)tool_.to_raw(), heater_on);
}
