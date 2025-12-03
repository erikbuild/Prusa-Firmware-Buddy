#include "inc/MarlinConfig.h"
#include <common/array_extensions.hpp>
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

/// returns std::array with VirtualToolIndex(i) at i-th index
constexpr auto get_default_mapping() {
    return stdext::make_iota_array<GcodeToolIndex::count, [](std::size_t i) {
        return std::variant<VirtualToolIndex, NoTool>(VirtualToolIndex::from_raw(i));
    }>();
}

ToolMapper tool_mapper;

ToolMapper::ToolMapper()
    : enabled(false)
    , gcode_to_virtual(get_default_mapping()) {}

ToolMapper &ToolMapper::operator=(const ToolMapper &other) {
    std::scoped_lock lock(mutex, other.mutex);
    this->enabled = other.enabled;
    for (size_t i = 0; i < std::size(gcode_to_virtual); i++) {
        this->gcode_to_virtual[i] = other.gcode_to_virtual[i];
    }
    return *this;
}

bool ToolMapper::set_mapping(GcodeToolIndex gcode_tool, VirtualToolIndex virtual_tool) {
    std::unique_lock lock(mutex);
    if (!is_tool_enabled(virtual_tool.to_raw())) {
        return false;
    }

    // if this virtual tool is already mapped to some gcode tool, remove this assignment
    auto maybe_gcode = to_gcode_unlocked(virtual_tool);
    auto *previous_gcode = std::get_if<GcodeToolIndex>(&maybe_gcode);
    if (previous_gcode) {
        gcode_to_virtual[previous_gcode->to_raw()] = NoTool {};
    }

    // do the mapping
    gcode_to_virtual[gcode_tool.to_raw()] = virtual_tool;
    return true;
}

void ToolMapper::set_unassigned(GcodeToolIndex gcode_tool) {
    std::unique_lock lock(mutex);
    set_unassigned_unlocked(gcode_tool);
}

void ToolMapper::set_unassigned_unlocked(GcodeToolIndex gcode_tool) {
    gcode_to_virtual[gcode_tool.to_raw()] = NoTool {};
}

void ToolMapper::set_enable(bool enable) {
    std::unique_lock lock(mutex);
    this->enabled = enable;
}

std::variant<VirtualToolIndex, NoTool> ToolMapper::to_virtual(GcodeToolIndex gcode_tool, bool ignore_enabled) const {
    std::unique_lock lock(mutex);
    if (ignore_enabled || enabled) {
        return gcode_to_virtual[gcode_tool.to_raw()];
    } else {
        return VirtualToolIndex::from_raw(gcode_tool.to_raw()); // no mapping
    }
}
std::variant<GcodeToolIndex, NoTool> ToolMapper::to_gcode(VirtualToolIndex virtual_tool) const {
    std::unique_lock lock(mutex);
    return to_gcode_unlocked(virtual_tool);
}

std::variant<GcodeToolIndex, NoTool> ToolMapper::to_gcode_unlocked(VirtualToolIndex virtual_tool) const {
    for (auto gcode_tool : GcodeToolIndex::all()) {
        if (gcode_to_virtual[gcode_tool.to_raw()] == std::variant<VirtualToolIndex, NoTool> { virtual_tool }) {
            return gcode_tool;
        }
    }
    return NoTool {};
}

void ToolMapper::reset() {
    std::unique_lock lock(mutex);
    gcode_to_virtual = get_default_mapping();
    enabled = false;
}

void ToolMapper::set_all_unassigned() {
    std::unique_lock lock(mutex);
    for (auto gcode_tool : GcodeToolIndex::all()) {
        set_unassigned_unlocked(gcode_tool);
    }
}

void ToolMapper::serialize(serialized_state_t &to) {
    // NOTE: We do not lock here now, as it is not possible other thread would be modifying
    // the objekt at this point (they do that before starting the print). If this ever changes
    // we should rethink this, this is called from default task, not ISR, so it might be ok to lock.
    to.enabled = enabled;
    for (auto gcode_tool : GcodeToolIndex::all()) {
        to.gcode_to_virtual[gcode_tool.to_raw()] = match(
            gcode_to_virtual[gcode_tool.to_raw()],
            [](VirtualToolIndex virtual_tool) { return virtual_tool.to_raw(); },
            [](NoTool) { return NO_TOOL_MAPPED; });
    }
}

void ToolMapper::deserialize(serialized_state_t &from) {
    std::unique_lock lock(mutex);
    enabled = from.enabled;
    for (auto gcode_tool : GcodeToolIndex::all()) {
        gcode_to_virtual[gcode_tool.to_raw()] = VirtualToolIndex::from_raw_notool(from.gcode_to_virtual[gcode_tool.to_raw()]);
    }
}

#endif
