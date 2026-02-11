#include <Marlin.h>
#include <gcode/gcode.h>
#include <feature/contactless_offset/contactless_offset.hpp>

/** \addtogroup G-Codes
 * @{
 */

/**
 * ### G426: Measure tool offset using contactless sensor
 *
 * Uses the induction sensor to measure the current tool's offset relative to a
 * reference position.
 *
 * #### Usage
 *
 *     G426 [F<speed>] [R<speed2>] [Z<height>] [D<diameter>]
 *
 * #### Parameters
 *
 * - `F` - First speed (v1) in mm/s (default: 7)
 * - `R` - Second speed (v2) in mm/s (default: 15)
 * - `Z` - Sensing height in mm above the sensor (default: from config)
 * - `D` - Scan diameter in mm (default: from config)
 */
void GcodeSuite::G426() {
    TEMPORARY_AUTO_REPORT_OFF(suspend_auto_report);
    SERIAL_ECHOLN("G426 contactless offset measurement");

    auto config = tool_offset::get_default_probing_config();

    if (parser.seenval('F')) {
        config.sensing_speed_slow = parser.value_float();
    }
    if (parser.seenval('R')) {
        config.sensing_speed_fast = parser.value_float();
    }
    if (parser.seenval('Z')) {
        config.sensing_z = parser.value_float();
    }
    if (parser.seenval('D')) {
        config.sensing_diameter = parser.value_float();
    }

    auto sensor = tool_offset::get_default_sensor();
    auto result = tool_offset::measure_current_tool_offset(config, *sensor);

    if (!result.has_value()) {
        SERIAL_ECHOLNPAIR("G426 failed: ", result.error());
        return;
    }

    SERIAL_ECHOPAIR("X offset: ", result->x, " mm");
    SERIAL_EOL();
    SERIAL_ECHOPAIR("Y offset: ", result->y, " mm");
    SERIAL_EOL();
    SERIAL_ECHOPAIR("Z offset: ", result->z, " mm");
    SERIAL_EOL();
}

/** @}*/
