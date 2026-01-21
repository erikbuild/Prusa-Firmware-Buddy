/// @file
#include "hotend.hpp"

#include <module/temperature.h>

#include <option/has_toolchanger.h>
#if HAS_TOOLCHANGER()
    #include "hotend/dummy_hotend.hpp"
#endif

static_assert(Hotend::temperature_uninitialized == TempInfo::celsius_uninitialized);

Hotend &Hotend::for_tool(uint8_t tool) {
    return for_tool(PhysicalToolIndex::from_raw_notool(tool));
}

Hotend &Hotend::for_tool(std::variant<PhysicalToolIndex, NoTool> tool) {
    return match(
        tool, //
        [](PhysicalToolIndex t) -> Hotend & { return for_tool(t); }, //
        [](NoTool) -> Hotend & {
#if HAS_TOOLCHANGER()
            static DummyHotend dummy;
            return dummy;
#else
            bsod("notool hotend");
#endif
        } //
    );
}
