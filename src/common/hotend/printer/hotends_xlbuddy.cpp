/// @file

#include <hotend/hotend/dwarf_hotend.hpp>
#include <utils/storage/strong_index_array.hpp>

Hotend &Hotend::for_tool(PhysicalToolIndex tool) {
    static StrongIndexArray<DwarfHotend, PhysicalToolIndex::count, PhysicalToolIndex, PhysicalToolIndex::to_raw_static> hotends {
        DwarfHotend(PhysicalToolIndex::from_raw(0)),
        DwarfHotend(PhysicalToolIndex::from_raw(1)),
        DwarfHotend(PhysicalToolIndex::from_raw(2)),
        DwarfHotend(PhysicalToolIndex::from_raw(3)),
        DwarfHotend(PhysicalToolIndex::from_raw(4)),
    };
    static_assert(PhysicalToolIndex::count == 5);

    return hotends[tool];
}
