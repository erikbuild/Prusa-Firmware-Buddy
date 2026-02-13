/// @file

#include <hotend/hotend/dwarf_hotend.hpp>
#include <utils/storage/strong_index_array.hpp>

Hotend &Hotend::for_tool(PhysicalToolIndex tool) {
    static constexpr DwarfHotend::Config hotend_config {
        // TODO: Get rid of the macros, put the values directly into this file
        .min_nozzle_temp = HEATER_XL_HOTEND_MINTEMP,
        .max_nozzle_temp = HEATER_XL_HOTEND_MAXTEMP,
    };
    static StrongIndexArray<DwarfHotend, PhysicalToolIndex::count, PhysicalToolIndex, PhysicalToolIndex::to_raw_static> hotends {
        DwarfHotend(PhysicalToolIndex::from_raw(0), &hotend_config),
        DwarfHotend(PhysicalToolIndex::from_raw(1), &hotend_config),
        DwarfHotend(PhysicalToolIndex::from_raw(2), &hotend_config),
        DwarfHotend(PhysicalToolIndex::from_raw(3), &hotend_config),
        DwarfHotend(PhysicalToolIndex::from_raw(4), &hotend_config),
    };
    static_assert(PhysicalToolIndex::count == 5);

    return hotends[tool];
}
