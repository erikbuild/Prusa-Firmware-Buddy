#include "config.hpp"
#include <config.h>
#include <printers.h>

#if PRINTER_IS_PRUSA_COREONE()
tool_offset::ProbingConfig tool_offset::get_default_probing_config() {
    return ProbingConfig {
        .sensor_position = { 257.f, 10.f, 0.f },
        .safe_z_height = 4.f,
        .sensing_z = .2f,
        .sensing_diameter = 10.f,
        .sensing_speed_slow = 20.f,
        .sensing_speed_fast = 30.f,
        .sweep_rest_time = 0.15f,
        .max_safe_temp = 180.f
    };
}
#else
    #error "No default probing config for this printer"
#endif
