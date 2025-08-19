#include "test_files.hpp"

#include <gcode_reader_any.hpp>
#include <gcode_info.hpp>
#include <catch2/catch.hpp>

namespace {

// Test both on plaintext and binary readers. They behave slightly differently
// on the outside. No need to do different compressions or so, though, these shoud be the same.
const std::vector<const char *> test_files = { NEW_PLAIN, NEW_BINARY, NEW_ENCRYPTED };

} // namespace

TEST_CASE("GCodeInfo") {
    for (const char *filename : test_files) {
        SECTION(std::string("Test-file: ") + filename) {
            AnyGcodeFormatReader reader(filename);
            REQUIRE(reader.is_open());

            GCodeInfo info;
            info.load(*reader.get());
            CHECK(info.is_loaded());
            CHECK(!info.has_error());

            CHECK(info.has_preview_thumbnail());
            CHECK(info.has_progress_thumbnail());
            CHECK(info.has_filament_described());
            if (filename != NEW_ENCRYPTED) { // Yes, pointer-compare is enough
                // BUG: These are in the encrypted section. The gcode info
                // doesn't open this (on purpose). But these are not accessible
                // for us in this case.
                CHECK(info.get_bed_preheat_temp() == 60);
                CHECK(info.get_hotend_preheat_temp() == 215);
            }
            CHECK(info.is_singletool_gcode());
            CHECK(info.UsedExtrudersCount() == 1);

            auto extruder_info = info.get_extruder_info(0);
            CHECK(extruder_info.used());
            CHECK(strcmp(extruder_info.filament_name->data(), "PLA") == 0);
            CHECK(!extruder_info.requires_hardened_nozzle.has_value());
            CHECK(!extruder_info.requires_high_flow_nozzle.has_value());
        }
    }
}
