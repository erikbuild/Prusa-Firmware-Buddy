/// @file
/// @brief Tool offset calibration (Z-offset via probing + XY-offset with tool_offset board)

#include "tool_offset_calibration.hpp"

#include <bitset>

#include <Marlin/src/gcode/gcode.h>
#include <Marlin/src/module/motion.h>
#include <Marlin/src/module/planner.h>
#include <Marlin/src/module/tool_change.h>
#include <Marlin/src/module/probe.h>
#include <Marlin/src/module/prusa/toolchanger.h>
#include <tool_index.hpp>
#include <tools_mapping.hpp>
#include <gcode/gcode_info.hpp>
#include <bsod.h>
#include <filament.hpp>
#include <config_store/store_instance.hpp>
#include <logging/log.hpp>
#include <random/random.h>
#include <mapi/parking.hpp>
#include <raii/scope_guard.hpp>
#include <nozzle_cleaner.hpp>
#include <feature/print_status_message/print_status_message_guard.hpp>
#include <feature/contactless_offset/contactless_offset.hpp>
#include <feature/pressure_advance/pressure_advance_config.hpp>
#include <utils/variant_utils.hpp>
#include <option/has_toolchanger.h>

#include <option/has_spool_join.h>
#if HAS_SPOOL_JOIN()
    #include <module/prusa/spool_join.hpp>
#endif

static_assert(HAS_TOOLCHANGER(), "Needs toolchanger");

LOG_COMPONENT_DEF(ToolOffsetCalib, logging::Severity::info);

namespace {
// INDX_TODO: Set preferred reference line positions
constexpr xy_pos_t POS_TOOL_0 = { 5.0f, 5.0f };
constexpr xy_pos_t POS_TOOL_LAST = { 45.0f, 5.0f };

// Safe Z height for travel moves between probes
constexpr float SAFE_Z_HEIGHT = 3.0f;

// Fallback temperatures if no filament is loaded
constexpr int16_t DEFAULT_CLEANING_TEMP = 220;
constexpr int16_t DEFAULT_PROBING_TEMP = 170;

struct ToolTemperatures {
    int16_t cleaning; // nozzle temp for purge/clean
    int16_t probing; // cooled-down temp for Z probing
};

/// Get nozzle temperatures for a physical tool from its loaded filament.
/// Uses tool mapping to find the gcode tool, then looks up the filament type.
ToolTemperatures get_tool_temperatures(PhysicalToolIndex tool) {
    const uint8_t gcode_raw = tools_mapping::to_gcode_tool(tool.to_raw());
    if (gcode_raw == tools_mapping::no_tool) {
        return { DEFAULT_CLEANING_TEMP, DEFAULT_PROBING_TEMP };
    }
    const FilamentType filament = config_store().get_filament_type(VirtualToolIndex::from_raw(gcode_raw));
    if (filament == FilamentType::none) {
        return { DEFAULT_CLEANING_TEMP, DEFAULT_PROBING_TEMP };
    }
    const auto params = filament.parameters();
    return { params.nozzle_temperature, params.nozzle_preheat_temperature };
}

/// Update the print status message with tool offset calibration progress
void set_calib_status(PrintStatusMessageGuard &guard, uint8_t tool, uint8_t current, uint8_t total) {
    guard.update<PrintStatusMessage::Type::tool_offset_calibrating>({
        .progress = { .current = static_cast<float>(current), .target = static_cast<float>(total) },
        .tool = tool,
    });
}

/// Return a random float in [-r_param, +r_param]
float random_jitter(uint8_t r_param) {
    const float normalized = rand_f_from_u(rand_u()); // [0.0, 1.0]
    return (normalized * 2.0f - 1.0f) * r_param; // [-r_param, +r_param]
}

/// Get the Z probe position based on the tool's order among mapped tools.
/// @param index 0-based position among mapped tools
/// @param total total number of mapped tools
xy_pos_t probe_position(uint8_t index, uint8_t total, uint8_t r_param) {
    const float t = (total <= 1) ? 0.0f : static_cast<float>(index) / static_cast<float>(total - 1);
    return {
        std::clamp(POS_TOOL_0.x + (POS_TOOL_LAST.x - POS_TOOL_0.x) * t + random_jitter(r_param), static_cast<float>(X_MIN_POS), static_cast<float>(X_MAX_POS)),
        std::clamp(POS_TOOL_0.y + (POS_TOOL_LAST.y - POS_TOOL_0.y) * t + random_jitter(r_param), static_cast<float>(Y_MIN_POS), static_cast<float>(Y_MAX_POS)),
    };
}

/// Probe Z at a given XY position, averaging multiple measurements.
/// Returns NaN on failure.
/// Strips the currently applied hotend offset to avoid accumulating old offsets.
float probe_z_at(const xy_pos_t &pos, uint8_t probe_count) {

    const float measured = probe_at_point(pos, PROBE_PT_NONE, 1, true, probe_count);
    if (std::isnan(measured)) {
        return measured;
    } else {
        return measured - hotend_currently_applied_offset.z;
    }
}

/// Park at nozzle cleaner and run the cleaning sequence.
/// @return true if cleaning succeeded, false on failure or abort
bool prepare_tool(PhysicalToolIndex tool) {
    const ToolTemperatures temps = get_tool_temperatures(tool);

    mapi::park(mapi::ZAction::absolute_move, mapi::ParkingPosition::from_xyz_pos({ { X_WASTEBIN_SAFE_POINT, Y_BRUSH_AVOID_POINT, SAFE_Z_HEIGHT } }));

    if (!nozzle_cleaner::load_and_execute(nozzle_cleaner::Sequence::eject_blob)) {
        return false;
    }

    thermalManager.setTargetHotend(temps.cleaning, tool);

    // Purge and clean at cleaning temperature
    thermalManager.wait_for_hotend(tool, false);
    if (!nozzle_cleaner::load_and_execute(nozzle_cleaner::Sequence::purge_clean)) {
        return false;
    }

    // Cool down to probing temperature with print fan on to speed it up
    thermalManager.setTargetHotend(temps.probing, tool);
    const uint16_t saved_fan_speed = thermalManager.get_fan_speed(0);
    thermalManager.set_fan_speed(0, 255);
    ScopeGuard restore_fan([&] {
        thermalManager.set_fan_speed(0, saved_fan_speed);
    });

    // Deep clean at probing temperature
    thermalManager.wait_for_hotend(tool, false);
    if (!nozzle_cleaner::load_and_execute(nozzle_cleaner::Sequence::deep_clean)) {
        return false;
    }

    // park out of cleaner area
    mapi::park(mapi::ZAction::absolute_move, mapi::ParkingPosition::from_xyz_pos({ { X_WASTEBIN_SAFE_POINT, Y_WASTEBIN_SAFE_POINT, SAFE_Z_HEIGHT } }));
    return true;
}

using ToolSet = std::bitset<PhysicalToolIndex::count>;

/// Collect the unique set of physical tools needed for the current print,
/// based on tool mapping and spool join configuration.
ToolSet collect_used_physical_tools() {
    ToolSet seen;

    // Walk each gcode tool that is used in the print
    auto &gcode_info = GCodeInfo::getInstance();
    for (auto gcode_tool : GcodeToolIndex::all()) {
        if (!gcode_info.get_extruder_info(gcode_tool).used()) {
            continue;
        }

        // Map gcode → virtual (respects tool mapper)
        auto virtual_tool = stdext::get_optional<VirtualToolIndex>(gcode_tool.to_virtual());
        while (virtual_tool) {
            seen.set(virtual_tool->to_physical().to_raw());

#if HAS_SPOOL_JOIN()
            virtual_tool = spool_join.get_spool_2(*virtual_tool);
#else
            virtual_tool = std::nullopt;
#endif
        }
    }

    return seen;
}

/// Collect all physically enabled tools.
ToolSet collect_all_enabled_tools() {
    ToolSet seen;
    for (auto tool : PhysicalToolIndex::all().skip_all_disabled()) {
        seen.set(tool.to_raw());
    }
    return seen;
}

/// Find the lowest set bit in a ToolSet, return as PhysicalToolIndex
PhysicalToolIndex first_tool(const ToolSet &set) {
    for (uint8_t i = 0; i < PhysicalToolIndex::count; i++) {
        if (set.test(i)) {
            return PhysicalToolIndex::from_raw(i);
        }
    }
    bsod("empty tool set");
}

/// Reset Z tool offsets to zero (runtime + EEPROM) to avoid corruption from previous prints
void reset_z_tool_offsets() {
    for (auto tool : PhysicalToolIndex::all()) {
        hotend_offset[tool].z = 0;
    }
    hotend_currently_applied_offset.z = 0;
    prusa_toolchanger.save_tool_offsets();
}

} // namespace

namespace tool_offset_calibration {

bool calibrate_xy_offset(PhysicalToolIndex tool, const tool_offset::ProbingConfig &config) {
    const auto selected_tool = stdext::get_optional<PhysicalToolIndex>(PhysicalToolIndex::currently_selected());
    if (!selected_tool.has_value()) {
        log_error(ToolOffsetCalib, "failed: no tool selected");
        return false;
    }
    if (tool != selected_tool.value()) {
        log_error(ToolOffsetCalib, "Selected tool must be picked");
        return false;
    }

    // Reset planner state
    planner.synchronize();
    planner.reset_position();

    // Disable PA to reduce filter delay during probe analysis
    pressure_advance::PressureAdvanceDisabler pa_disabler;

    // Perform the measurement for picked tool
    auto sensor = tool_offset::get_default_sensor();
    tool_offset::ToolOffset current_ho = {
        .x = hotend_offset[tool].x,
        .y = hotend_offset[tool].y,
        .z = hotend_offset[tool].z,
    };

    auto offset_for_measurement = current_ho;
    offset_for_measurement.x = std::clamp(offset_for_measurement.x, static_cast<float>(X_MIN_OFFSET), static_cast<float>(X_MAX_OFFSET));
    offset_for_measurement.y = std::clamp(offset_for_measurement.y, static_cast<float>(Y_MIN_OFFSET), static_cast<float>(Y_MAX_OFFSET));
    // Zero hotend offset and currently applied offset
    reset_hotend_offset(tool);
    hotend_currently_applied_offset = xyz_pos_t {};

    // Check printer physical limits and shift the sensor position if needed to avoid crashes
    tool_offset::ProbingConfig sensor_corrected_config = config;
    sensor_corrected_config.sensor_position.x = std::max(sensor_corrected_config.sensor_position.x, X_MIN_POS + config.sensing_diameter / 2.0f + X_MAX_OFFSET);
    sensor_corrected_config.sensor_position.x = std::min(sensor_corrected_config.sensor_position.x, X_MAX_POS - config.sensing_diameter / 2.0f - X_MAX_OFFSET);
    offset_for_measurement.x += sensor_corrected_config.sensor_position.x - config.sensor_position.x;
    sensor_corrected_config.sensor_position.y = std::max(sensor_corrected_config.sensor_position.y, Y_MIN_POS + config.sensing_diameter / 2.0f + Y_MAX_OFFSET);
    sensor_corrected_config.sensor_position.y = std::min(sensor_corrected_config.sensor_position.y, Y_MAX_POS - config.sensing_diameter / 2.0f - Y_MAX_OFFSET);
    offset_for_measurement.y += sensor_corrected_config.sensor_position.y - config.sensor_position.y;

    for (int i = 0; i < 3; i++) {
        log_info(ToolOffsetCalib, "XY offset measurement attempt %d", i + 1);
        auto result = tool_offset::measure_current_tool_offset(sensor_corrected_config, *sensor, offset_for_measurement);
        if (result.has_value()) {
            log_info(ToolOffsetCalib, "Measured XY offset: X=%.3f Y=%.3f", static_cast<double>(result->x), static_cast<double>(result->y));
            // Store newly measured offsets only for XY, keep actual for Z
            hotend_offset[tool].x = result->x + config.sensor_position.x - sensor_corrected_config.sensor_position.x; // Correct for any sensor position shift
            hotend_offset[tool].y = result->y + config.sensor_position.y - sensor_corrected_config.sensor_position.y; // Correct for any sensor position shift
            hotend_offset[tool].z = current_ho.z;
            prusa_toolchanger.save_tool_offset(tool);
            hotend_currently_applied_offset = xyz_pos_t { result->x, result->y, current_ho.z };
            return true;
        } else {
            log_error(ToolOffsetCalib, "Measurement failed: %s", result.error());
            // retrials performed with 0 offsets
            offset_for_measurement.x = sensor_corrected_config.sensor_position.x - config.sensor_position.x;
            offset_for_measurement.y = sensor_corrected_config.sensor_position.y - config.sensor_position.y;
        }
    }
    // Calibration failed after retries, restore original offsets
    hotend_offset[tool].x = current_ho.x;
    hotend_offset[tool].y = current_ho.y;
    hotend_offset[tool].z = current_ho.z;
    prusa_toolchanger.save_tool_offset(tool);
    hotend_currently_applied_offset = xyz_pos_t { current_ho.x, current_ho.y, current_ho.z };
    return false;
}

bool run(uint8_t r_param, uint8_t probe_count) {
    PrintStatusMessageGuard status_guard;
    const auto probing_config = tool_offset::get_default_probing_config();

    reset_z_tool_offsets(); // Clear old Z offsets to avoid interference with calibration

    log_info(ToolOffsetCalib, "Starting tool offset calibration");

    // Restore the originally picked tool on any exit path
    const auto original_tool = PhysicalToolIndex::currently_selected();
    ScopeGuard restore_tool([&] {
        tool_change(stdext::to_variant(original_tool), tool_return_t::no_return);
    });

    if (!GcodeSuite::G28_no_parser(true, true, true, G28Flags { .only_if_needed = true })) {
        log_error(ToolOffsetCalib, "Homing failed");
        return false;
    }

    // Collect the physical tools we need to calibrate from the active tool mapping.
    // Fall back to all enabled tools when no mapping is active (e.g. debug/standalone use).
    ToolSet mapped_tools = collect_used_physical_tools();
    if (mapped_tools.none()) {
        mapped_tools = collect_all_enabled_tools();
    }

    const auto num_tools = static_cast<uint8_t>(mapped_tools.count());
    if (num_tools == 0) {
        log_error(ToolOffsetCalib, "No tools found");
        return false;
    }

    log_info(ToolOffsetCalib, "Calibrating %u tool(s)", num_tools);

    const PhysicalToolIndex first = first_tool(mapped_tools);

    struct ProbeResult {
        float z;
        xy_pos_t pos;
    };
    uint8_t step = 0;

    // Helper: probe Z at the given mapped index position (with jitter)
    auto probe_at = [&](PhysicalToolIndex tool, uint8_t mapped_index) -> std::optional<ProbeResult> {
        const xy_pos_t pos = probe_position(mapped_index, num_tools, r_param);
        log_info(ToolOffsetCalib, "Probe count: %d", probe_count);
        const float z = probe_z_at(pos, probe_count);
        if (std::isnan(z)) {
            return std::nullopt;
        }
        log_info(ToolOffsetCalib, "Tool %u Z=%.3f at (%.1f, %.1f)", tool.to_raw(), static_cast<double>(z), static_cast<double>(pos.x), static_cast<double>(pos.y));
        do_blocking_move_to_z(SAFE_Z_HEIGHT);
        return ProbeResult { z, pos };
    };

    // Establish reference line with the first mapped tool
    set_calib_status(status_guard, first.to_raw(), ++step, num_tools);
    tool_change(stdext::to_variant(first), tool_return_t::no_return);

    ProbeResult ref_first;
    ProbeResult ref_last;
    {
        const int16_t first_saved_temp = thermalManager.degTargetHotend(first);
        ScopeGuard restore_temp([&] {
            // Restore first tool's target temperature before moving on
            thermalManager.setTargetHotend(first_saved_temp, first);
        });

        if (!prepare_tool(first)) {
            return false;
        }

        // Probe at first position (index 0)
        const auto ref_first_opt = probe_at(first, 0);
        if (!ref_first_opt) {
            return false;
        }
        ref_first = *ref_first_opt;
        ref_last = ref_first;

        if (num_tools != 1) {
            // Probe at last position (with the same first tool)
            const auto ref_last_opt = probe_at(first, num_tools - 1);
            if (!ref_last_opt) {
                return false;
            }
            ref_last = *ref_last_opt;

            log_info(ToolOffsetCalib, "Reference line: Z_first=%.3f at (%.1f,%.1f) Z_last=%.3f at (%.1f,%.1f)",
                static_cast<double>(ref_first.z), static_cast<double>(ref_first.pos.x), static_cast<double>(ref_first.pos.y),
                static_cast<double>(ref_last.z), static_cast<double>(ref_last.pos.x), static_cast<double>(ref_last.pos.y));
        }

        // XY calibration for the first tool
        if (!calibrate_xy_offset(first, probing_config)) {
            return false;
        }
    } // End of temp scope guard

    if (mapped_tools.count() == 1) {
        hotend_offset[first].z = 0.0f;
        prusa_toolchanger.save_tool_offsets();
        return true;
    }

    // The interpolation parameter t is computed from the actual probed X positions
    // to account for the random jitter applied to each probe point.
    const float ref_x_span = ref_last.pos.x - ref_first.pos.x;

    // First tool offset is 0 by definition
    hotend_offset[first].z = 0.0f;

    uint8_t mapped_index = 0; // first tool was index 0
    for (uint8_t i = 0; i < PhysicalToolIndex::count; i++) {
        if (!mapped_tools.test(i)) {
            continue;
        }
        mapped_index++;
        if (i == first.to_raw()) {
            continue;
        }
        const auto tool = PhysicalToolIndex::from_raw(i);

        set_calib_status(status_guard, tool.to_raw(), ++step, num_tools);
        tool_change(stdext::to_variant(tool), tool_return_t::no_return);

        ProbeResult result;
        {
            const int16_t saved_temp = thermalManager.degTargetHotend(tool);
            ScopeGuard restore_temp([&] {
                thermalManager.setTargetHotend(saved_temp, tool);
            });

            if (!prepare_tool(tool)) {
                return false;
            }

            const auto result_opt = probe_at(tool, mapped_index - 1);
            if (!result_opt) {
                return false;
            }
            result = *result_opt;

            if (!calibrate_xy_offset(tool, probing_config)) {
                return false;
            }
        }
        // Interpolate expected Z on the reference line at the actual probed X
        const float t = result.pos.x - ref_first.pos.x;
        const float z_expected = ref_first.z + (ref_last.z - ref_first.z) * (t / ref_x_span);
        const float z_offset = z_expected - result.z; // Inverted, because +Z is down
        hotend_offset[tool].z = z_offset;
        log_info(ToolOffsetCalib, "Tool %u Z offset=%.3f (measured=%.3f expected=%.3f)", i, static_cast<double>(z_offset), static_cast<double>(result.z), static_cast<double>(z_expected));
    }

    prusa_toolchanger.save_tool_offsets();

    log_info(ToolOffsetCalib, "Tool offset calibration done");
    return true;
}

} // namespace tool_offset_calibration
