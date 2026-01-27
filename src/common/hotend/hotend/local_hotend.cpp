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
    nozzle_temp_ = marlin_temptable_lookup(local_config_.nozzle_temp_table, thermalManager.temp_hotend[tool_].raw);

    // !!! MUST be called after temps are set properly
    BaseHotend::manage();
}
