/// @file

#include <tool/tool/standard_fff_physical_tool.hpp>
#include <tool/hotend/hotend/local_hotend.hpp>
#include <utils/storage/strong_index_array.hpp>
#include <module/thermistor/thermistors.h>
#include <hw_configuration.hpp>

#if !PRINTER_IS_PRUSA_MK3_5()
static MarlinTempTable heatbreak_temptable() {
    if (buddy::hw::Configuration::Instance().needs_heatbreak_thermistor_table_5()) {
        return TT_NAME(5);
    } else {
        return TT_NAME(TEMP_SENSOR_HEATBREAK);
    }
}
#endif

PhysicalTool &PhysicalTool::for_index(PhysicalToolIndex) {
    static const LocalHotend::Config hotend_config {
        .base_config {
            // TODO: Get rid of the macros, put the values directly into this file
            .min_nozzle_temp = HEATER_0_MINTEMP,
            .max_nozzle_temp = HEATER_0_MAXTEMP,
        },
            .nozzle_temp_table = TT_NAME(THERMISTOR_HEATER_0),
#if !PRINTER_IS_PRUSA_MK3_5()
            .heatbreak_temp_table = heatbreak_temptable(),
#endif
            .nozzle_heater_marlin_pin = MARLIN_PIN(HEAT0),
#if PRINTER_IS_PRUSA_MK3_5()
            .auto_fan_pin = MARLIN_PIN(AUTOFAN),
#endif
            .nozzle_heater_soft_pwm = false,
    };
    static StandardFFFPhysicalTool<LocalHotend> tool { PhysicalToolIndex::from_raw(0), &hotend_config };
    static_assert(PhysicalToolIndex::count == 1);

    return tool;
}
