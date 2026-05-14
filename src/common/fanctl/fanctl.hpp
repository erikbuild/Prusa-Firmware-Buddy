#pragma once
#include <fanctl/CFanCtlCommon.hpp>
#include "printers.h"
#include <device/board.h>
#include <cstddef>
#include <tool_index.hpp>
#include <option/has_indx.h>
#include <option/xl_enclosure_support.h>

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

#if HAS_INDX()
    // Auxiliary dock fan, wired to the xBuddy NEXTRUDER connector's print-fan
    // pin and using the same fan controller as the C1 print fan. The print fan
    // itself lives on the INDX head so that pin is free here.
    static CFanCtlCommon &dock_fan();
#endif

    static void tick();
};
