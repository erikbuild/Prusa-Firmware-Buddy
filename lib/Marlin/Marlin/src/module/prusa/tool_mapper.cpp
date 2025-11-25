#include "inc/MarlinConfig.h"
#include <cstddef>
#include <iterator>
#include <mutex>
#include <option/has_mmu2.h>
#include <option/has_tool_mapping.h>
#include <option/has_toolchanger.h>

#if HAS_TOOL_MAPPING()

    #include "module/prusa/tool_mapper.hpp"
    #include "mmu2_toolchanger_common.hpp"
    #if HAS_TOOLCHANGER()
        #include <module/prusa/toolchanger.h>
    #endif

// This value is important only for XL, for MMU it should just be something bigger than 5 (num of slots)
uint8_t get_invalid_tool_number() {
    #if HAS_TOOLCHANGER()
    return PrusaToolChanger::MARLIN_NO_TOOL_PICKED;
    #elif HAS_MMU2()
    return 6; // MMU has 5 slots
    #endif
}

ToolMapper tool_mapper;

ToolMapper::ToolMapper() {
    reset();
}

ToolMapper &ToolMapper::operator=(const ToolMapper &other) {
    std::scoped_lock lock(mutex, other.mutex);
    this->enabled = other.enabled;
    for (size_t i = 0; i < std::size(gcode_to_virtual); i++) {
        this->gcode_to_virtual[i] = other.gcode_to_virtual[i];
    }
    return *this;
}

bool ToolMapper::set_mapping(uint8_t gcode_tool, uint8_t virtual_tool) {
    std::unique_lock lock(mutex);
    // virtual tool is enabled and valid
    if (virtual_tool >= EXTRUDERS || !is_tool_enabled(virtual_tool)) {
        return false;
    }

    // check that gcode tool is valid as well
    if (gcode_tool >= EXTRUDERS || gcode_tool == get_invalid_tool_number()) {
        return false;
    }

    // if this virtual tool is already mapped to some gcode tool, remove this assignment
    uint8_t previous_gcode = to_gcode_unlocked(virtual_tool);
    if (previous_gcode != NO_TOOL_MAPPED) {
        gcode_to_virtual[previous_gcode] = NO_TOOL_MAPPED;
    }

    // do the mapping
    gcode_to_virtual[gcode_tool] = virtual_tool;
    return true;
}

bool ToolMapper::set_unassigned(uint8_t gcode_tool) {
    std::unique_lock lock(mutex);
    return set_unassigned_unlocked(gcode_tool);
}

bool ToolMapper::set_unassigned_unlocked(uint8_t gcode_tool) {
    // check that gcode tool is valid
    if (gcode_tool >= EXTRUDERS || gcode_tool == get_invalid_tool_number()) {
        return false;
    }

    gcode_to_virtual[gcode_tool] = NO_TOOL_MAPPED;
    return true;
}

void ToolMapper::set_enable(bool enable) {
    std::unique_lock lock(mutex);
    this->enabled = enable;
}

uint8_t ToolMapper::to_virtual(uint8_t gcode_tool, bool ignore_enabled) const {
    std::unique_lock lock(mutex);
    if ((ignore_enabled || enabled) && gcode_tool < std::size(gcode_to_virtual)) {
        return gcode_to_virtual[gcode_tool];
    } else {
        return gcode_tool; // no maping
    }
}
uint8_t ToolMapper::to_gcode(uint8_t virtual_tool) const {
    std::unique_lock lock(mutex);
    return to_gcode_unlocked(virtual_tool);
}

uint8_t ToolMapper::to_gcode_unlocked(uint8_t virtual_tool) const {
    for (size_t i = 0; i < std::size(gcode_to_virtual); i++) {
        if (gcode_to_virtual[i] == virtual_tool) {
            return i;
        }
    }
    return NO_TOOL_MAPPED;
}

void ToolMapper::reset() {
    std::unique_lock lock(mutex);
    for (size_t i = 0; i < std::size(gcode_to_virtual); i++) {
        gcode_to_virtual[i] = i;
    }
    enabled = false;
}

void ToolMapper::set_all_unassigned() {
    std::unique_lock lock(mutex);
    for (int8_t e = 0; e < EXTRUDERS; e++) {
        set_unassigned_unlocked(e);
    }
}

void ToolMapper::serialize(serialized_state_t &to) {
    // NOTE: We do not lock here now, as it is not possible other thread would be modifying
    // the objekt at this point (they do that before starting the print). If this ever changes
    // we should rethink this, this is called from default task, not ISR, so it might be ok to lock.
    to.enabled = enabled;
    for (int8_t e = 0; e < EXTRUDERS; e++) {
        to.gcode_to_virtual[e] = gcode_to_virtual[e];
    }
}

void ToolMapper::deserialize(serialized_state_t &from) {
    std::unique_lock lock(mutex);
    enabled = from.enabled;
    for (int8_t e = 0; e < EXTRUDERS; e++) {
        gcode_to_virtual[e] = from.gcode_to_virtual[e];
    }
}

#endif
