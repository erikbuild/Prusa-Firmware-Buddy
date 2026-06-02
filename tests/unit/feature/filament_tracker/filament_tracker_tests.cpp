#include <catch2/catch_test_macros.hpp>

#include <feature/filament_tracker/filament_tracker.hpp>

using namespace buddy;

TEST_CASE("FilamentTracker") {
    constexpr PhysicalToolIndex pt { 0 };
    constexpr VirtualToolIndex vt { 0 };

    FilamentTracker ft;

    // Should be zeroed out at the beginning
    CHECK(ft.get_retracted_distance(pt) == std::nullopt);
    CHECK(ft.get_extruded_distance(vt) == 0);

    ft.track_extruder_move(vt, 3);
    CHECK(ft.get_retracted_distance(pt) == 0);
    CHECK(ft.get_extruded_distance(vt) == 3);

    ft.track_extruder_move(vt, -2);
    CHECK(ft.get_retracted_distance(pt) == 2);
    CHECK(ft.get_extruded_distance(vt) == 3);

    ft.track_extruder_move(vt, 2);
    CHECK(ft.get_retracted_distance(pt) == 0);
    CHECK(ft.get_extruded_distance(vt) == 3);

    ft.track_extruder_move(vt, 1);
    CHECK(ft.get_retracted_distance(pt) == 0);
    CHECK(ft.get_extruded_distance(vt) == 4);

    ft.track_extruder_move(vt, -FilamentTracker::extruder_to_nozzle_distance / 2);
    CHECK(ft.get_retracted_distance(pt) == FilamentTracker::extruder_to_nozzle_distance / 2);
    CHECK(ft.get_extruded_distance(vt) == 4);

    ft.track_extruder_move(vt, -FilamentTracker::extruder_to_nozzle_distance / 2);
    CHECK(ft.get_retracted_distance(pt) == std::nullopt);
    CHECK(ft.get_extruded_distance(vt) == 4);

    ft.track_extruder_move(vt, FilamentTracker::extruder_to_nozzle_distance + 3.5f);
    CHECK(ft.get_retracted_distance(pt) == 0);
    CHECK(ft.get_extruded_distance(vt) == 7 /* .5 */);

    ft.track_extruder_move(vt, 0.5);
    CHECK(ft.get_retracted_distance(pt) == 0);
    CHECK(ft.get_extruded_distance(vt) == 8);

    ft.track_extruder_move(vt, -0.5f);
    CHECK(ft.get_retracted_distance(pt) == 0.5f);
    CHECK(ft.get_extruded_distance(vt) == 8);

    ft.track_extruder_move(vt, 0.75f);
    CHECK(ft.get_retracted_distance(pt) == 0);
    CHECK(ft.get_extruded_distance(vt) == 8 /* .25 */);
}
