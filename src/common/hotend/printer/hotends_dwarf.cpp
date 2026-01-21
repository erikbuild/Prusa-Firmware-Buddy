/// @file

#include <hotend/hotend/local_hotend.hpp>
#include <utils/storage/strong_index_array.hpp>

Hotend &Hotend::for_tool(PhysicalToolIndex) {
    static LocalHotend hotend { PhysicalToolIndex::from_raw(0) };
    static_assert(PhysicalToolIndex::count == 1);

    return hotend;
}
