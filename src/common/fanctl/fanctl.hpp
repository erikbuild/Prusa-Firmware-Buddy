#pragma once
#include "CFanCtlCommon.hpp"
#include "printers.h"
#include <device/board.h>
#include <cstddef>
#include <tool_index.hpp>

class Fans {
    Fans() = default;
    Fans(const Fans &) = default;

public:
    [[deprecated("Use the ToolIndex overload")]]
    static CFanCtlCommon &print(size_t index);

    inline static CFanCtlCommon &print(PhysicalToolIndex tool) {
        return print(tool.to_raw());
    }

    [[deprecated("Use the ToolIndex overload")]]
    static CFanCtlCommon &heat_break(size_t index);

    inline static CFanCtlCommon &heat_break(PhysicalToolIndex tool) {
        return heat_break(tool.to_raw());
    }

#if XL_ENCLOSURE_SUPPORT() // XLBOARD has CFanCtlPuppy and additional enclosure fan, but DWARF has only normal CFanCtls
    static CFanCtlCommon &enclosure();
#endif
    static void tick();
};
