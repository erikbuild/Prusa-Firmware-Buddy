/// @file

#include <hotend/hotend/local_hotend.hpp>
#include <utils/storage/strong_index_array.hpp>
#include <module/thermistor/thermistors.h>

Hotend &Hotend::for_tool(PhysicalToolIndex) {
    static constexpr LocalHotend::Config hotend_config {
        .base_config {
            // TODO: Get rid of the macros, put the values directly into this file
            .min_nozzle_temp = HEATER_0_MINTEMP,
            .max_nozzle_temp = HEATER_0_MAXTEMP,
        },
        .nozzle_temp_table = TT_NAME(THERMISTOR_HEATER_0),
    };
    static LocalHotend hotend { PhysicalToolIndex::from_raw(0), &hotend_config };
    static_assert(PhysicalToolIndex::count == 1);

    return hotend;
}
