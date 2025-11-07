#include <catch2/catch.hpp>

#include <feature/gcode_exception/gcode_exception.hpp>

TEST_CASE("gcode_exception::naive_unhandled", "[gcode_exception]") {
    auto &gce = gcode_exceptions();

    CHECK(!gce.is_unwinding());

    {
        GCodeExceptionHandler r1 { GCEHandlerExtent::any_move, [&] { FAIL(); } };
        GCodeExceptionHandler r2 { GCEHandlerExtent::any_move, [&] { FAIL(); } };

        gce.throw_unhandled();
        CHECK(gce.is_unwinding());
    }

    CHECK(gce.finish_unwinding_unhandled_exception());
    CHECK(!gce.is_unwinding());
}

TEST_CASE("gcode_exception::throw_nested", "[gcode_exception]") {
    auto &gce = gcode_exceptions();
    bool recovered = false;

    {
        GCodeExceptionHandler outer { GCEHandlerExtent::any_move, [&] { FAIL(); } };

        {
            GCodeExceptionHandler inner { GCEHandlerExtent::any_move, [&] { recovered = true; } };
            gce.throw_at(&inner);
            CHECK(gce.is_unwinding());
            CHECK(!recovered);
        }

        CHECK(!gce.is_unwinding());
        CHECK(recovered);
    }
}

TEST_CASE("gcode_exception::throw_outer", "[gcode_exception]") {
    auto &gce = gcode_exceptions();
    bool recovered = false;

    CHECK_NOTHROW(gce.report_xyz_move());

    {
        GCodeExceptionHandler outer { GCEHandlerExtent::any_move, [&] { recovered = true; } };

        /// XYZ moves are permitted in any_move regions
        CHECK_NOTHROW(gce.report_xyz_move());

        {
            GCodeExceptionHandler inner { GCEHandlerExtent::extruder_only, [&] { FAIL(); } };

            // XYZ moves are not allowed inside extruder_only regions
            CHECK_THROWS(gce.report_xyz_move());

            gce.throw_at(&outer);
            CHECK(gce.is_unwinding());
        }

        CHECK(gce.is_unwinding());
        CHECK(!recovered);
    }

    CHECK(recovered);
    CHECK(!gce.is_unwinding());
}

TEST_CASE("gcode_exception::throw_unhandled", "[gcode_exception]") {
    auto &gce = gcode_exceptions();

    {
        GCodeExceptionHandler outer { GCEHandlerExtent::any_move, [&] { FAIL(); } };

        {
            GCodeExceptionHandler inner { GCEHandlerExtent::any_move, [&] { FAIL(); } };
            gce.throw_unhandled();
            CHECK(gce.is_unwinding());
        }

        CHECK(gce.is_unwinding());

        // Not allowed when any handlers are active
        CHECK_THROWS(gce.finish_unwinding_unhandled_exception());
    }

    CHECK(gce.is_unwinding());
    CHECK(gce.finish_unwinding_unhandled_exception());
    CHECK(!gce.is_unwinding());
}

TEST_CASE("gcode_exception::throw_multi", "[gcode_exception]") {
    auto &gce = gcode_exceptions();
    bool recovered = false;

    {
        GCodeExceptionHandler outer { GCEHandlerExtent::any_move, [&] { recovered = true; } };
        gce.throw_at(&outer);
        CHECK(gce.is_unwinding());

        {
            GCodeExceptionHandler inner { GCEHandlerExtent::any_move, [&] { FAIL(); } };
            gce.throw_at(&inner);
            CHECK(gce.is_unwinding());
        }

        CHECK(gce.is_unwinding());
        CHECK(!recovered);
    }

    CHECK(!gce.is_unwinding());
    CHECK(recovered);

    // Even though we called trigger() twice, there was actually just one quickstop
    CHECK(gce.throw_count() == 1);
}

TEST_CASE("gcode_exception::throw_multi_unhandled", "[gcode_exception]") {
    auto &gce = gcode_exceptions();

    {
        GCodeExceptionHandler outer { GCEHandlerExtent::any_move, [&] { FAIL(); } };
        gce.throw_unhandled();
        CHECK(gce.is_unwinding());

        {
            GCodeExceptionHandler inner { GCEHandlerExtent::any_move, [&] { FAIL(); } };
            gce.throw_at(&inner);
            CHECK(gce.is_unwinding());
        }

        CHECK(gce.is_unwinding());
    }

    CHECK(gce.is_unwinding());
    CHECK(gce.finish_unwinding_unhandled_exception());
    CHECK(!gce.is_unwinding());
}
