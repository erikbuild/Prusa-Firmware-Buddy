#pragma once

#include <freertos/mutex.hpp>
#include "inc/MarlinConfig.h"
#include <limits>
#include <mutex>
#include <option/has_tool_mapping.h>

#if HAS_TOOL_MAPPING()

/**
 * @brief Tool mapper allows user to assign tools in GUI/GCODE to virtual tools, depending on where he has filament he wants to use
 *
 * @note  Gcode tool - Tool that gcode refers to (eg. T1)
 *        Virtual tool - Abstraction above physical tool to enable filament multiplexing
 *        Mapping of gcode tool 1 to virtual tool 3 means that whenever gcode requests tool 1, printer will actually use tool 3
 */
class ToolMapper {
public:
    ToolMapper();

    ToolMapper &operator=(const ToolMapper &other);

    /// Create new mapping of tool
    bool set_mapping(uint8_t gcode_tool, uint8_t virtual_tool);

    /// Enable or disable all tool mappings
    void set_enable(bool enable);

    /// true when mapping is enabled
    inline bool is_enabled() {
        std::unique_lock lock(mutex);
        return enabled;
    }

    /// Convert gcode tool to virtual
    /// note: might return NO_TOOL_MAPPED, so check for this value
    [[nodiscard]] uint8_t to_virtual(uint8_t gcode_tool, bool ignore_enabled = false) const;

    /// Convert virtual tool to gcode
    [[nodiscard]] uint8_t to_gcode(uint8_t virtual_tool) const;

    /// Reset all tool mapping
    void reset();

    void set_all_unassigned();
    bool set_unassigned(uint8_t gcode_tool);

    // This is special tool identifier, that says that this tool is not mapped to any tool, and is threfore disabled by tool mapping
    static constexpr auto NO_TOOL_MAPPED = std::numeric_limits<uint8_t>::max();

    // Container with serialized state of tool mapping
    struct __attribute__((packed)) serialized_state_t {
        bool enabled;
        uint8_t gcode_to_virtual[EXTRUDERS];
    };

    // serialize state into packed structure (for power panic)
    void serialize(serialized_state_t &to);

    // deserialize state into packed structure (after power panic)
    void deserialize(serialized_state_t &from);

private:
    [[nodiscard]] uint8_t to_gcode_unlocked(uint8_t virtual_tool) const;
    bool set_unassigned_unlocked(uint8_t gcode_tool);

    mutable freertos::Mutex mutex;
    bool enabled;
    uint8_t gcode_to_virtual[EXTRUDERS];
};

extern ToolMapper tool_mapper;

#endif
