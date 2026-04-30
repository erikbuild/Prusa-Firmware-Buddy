#include "clo_config.hpp"
#include <config.h>
#include <printers.h>

#if PRINTER_IS_PRUSA_COREONE()
namespace tool_offset::detail {
static constexpr float sensor_x = 257.f;
static constexpr float sensor_y = Y_MAX_PRINT_POS - 197.5f; // CAD position 197.5mm from the homing position, Y homing position is Y_MAX_PRINT_POS
static constexpr float sensing_diameter = 6.f;
} // namespace tool_offset::detail
#elif PRINTER_IS_PRUSA_COREONEL()
// So far only copy from COREONE INDX
// TODO update values for Core ONEL INDX once the values are known
namespace tool_offset::detail {
static constexpr float sensor_x = 257.f;
static constexpr float sensor_y = Y_MAX_PRINT_POS - 197.5f;
static constexpr float sensing_diameter = 10.f;
} // namespace tool_offset::detail
#else
    #error "No default probing config for this printer"
#endif

tool_offset::ProbingConfig tool_offset::get_default_probing_config() {
    return ProbingConfig {
        .sensor_position = { detail::sensor_x, detail::sensor_y, 0.f }, // mm
        .safe_z_height = 4.f,
        .sensing_z = 0.2f,
        .sensing_diameter = detail::sensing_diameter,
        .sensing_speed_slow = 20.f,
        .sensing_speed_fast = 30.f,
        .sweep_rest_time = 0.35f,
        .max_safe_temp = 180.f,
        .symmetry_trim_fraction = 0.5f,
    };
}

static_assert(tool_offset::detail::sensor_x + tool_offset::detail::sensing_diameter / 2.0f <= X_MAX_POS, "Sensor position definition exceeds printer's physical limits");
static_assert(tool_offset::detail::sensor_y + tool_offset::detail::sensing_diameter / 2.0f <= Y_MAX_POS, "Sensor position definition exceeds printer's physical limits");
static_assert(tool_offset::detail::sensor_x - tool_offset::detail::sensing_diameter / 2.0f >= X_MIN_POS, "Sensor position definition exceeds printer's physical limits");
static_assert(tool_offset::detail::sensor_y - tool_offset::detail::sensing_diameter / 2.0f >= Y_MIN_POS, "Sensor position definition exceeds printer's physical limits");
