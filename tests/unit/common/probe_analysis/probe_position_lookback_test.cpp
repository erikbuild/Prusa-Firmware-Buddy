#include <catch2/catch_test_macros.hpp>
#include "probe_position_lookback.hpp"
#include "math.h"

using namespace buddy;

class ProbePositionLookback : public ProbePositionLookbackBase {

public:
    Sample current_sample;

    Sample generate_sample() const final {
        return current_sample;
    }

    void add_sample(uint32_t time_us, float x_pos, float y_pos, float z_pos, float e_pos) {
        ProbePositionLookbackBase::add_sample(Sample { .time = time_us, .x = x_pos, .y = y_pos, .z = z_pos, .e = e_pos });
    }
};

TEST_CASE("probe_position_lookback_basic") {
    ProbePositionLookback l;

    l.add_sample(1000, 1, 10, 100, 1000);
    l.add_sample(2000, 20, 30, 200, 2000);
    l.add_sample(3000, 30, 40, 300, 3000);
    l.add_sample(4000, 20, 30, 200, 2000);

    l.current_sample = {
        .time = 5000,
        .x = 35,
        .y = 45,
        .z = 350,
        .e = 3500,
    };

    auto s1 = l.get_position_at(1000);
    REQUIRE(s1.x == 1);
    REQUIRE(s1.y == 10);
    REQUIRE(s1.z == 100);
    REQUIRE(s1.e == 1000);

    auto s2 = l.get_position_at(1100);
    REQUIRE(s2.x == 2.9f);
    REQUIRE(s2.y == 12.0f);
    REQUIRE(s2.z == 110.0f);
    REQUIRE(s2.e == 1100.0f);

    auto s3 = l.get_position_at(1500);
    REQUIRE(s3.x == 10.5);
    REQUIRE(s3.y == 20);
    REQUIRE(s3.z == 150);
    REQUIRE(s3.e == 1500);

    auto s4 = l.get_position_at(2000);
    REQUIRE(s4.x == 20);
    REQUIRE(s4.y == 30);
    REQUIRE(s4.z == 200);
    REQUIRE(s4.e == 2000);

    auto s5 = l.get_position_at(2500);
    REQUIRE(s5.x == 25);
    REQUIRE(s5.y == 35);
    REQUIRE(s5.z == 250);
    REQUIRE(s5.e == 2500);

    auto s6 = l.get_position_at(3000);
    REQUIRE(s6.x == 30);
    REQUIRE(s6.y == 40);
    REQUIRE(s6.z == 300);
    REQUIRE(s6.e == 3000);

    auto s7 = l.get_position_at(3500);
    REQUIRE(s7.x == 25);
    REQUIRE(s7.y == 35);
    REQUIRE(s7.z == 250);
    REQUIRE(s7.e == 2500);

    auto s8 = l.get_position_at(4000);
    REQUIRE(s8.x == 20);
    REQUIRE(s8.y == 30);
    REQUIRE(s8.z == 200);
    REQUIRE(s8.e == 2000);

    auto s9 = l.get_position_at(4500);
    REQUIRE(s9.x == 27.5f);
    REQUIRE(s9.y == 37.5f);
    REQUIRE(s9.z == 275.0f);
    REQUIRE(s9.e == 2750.0f);

    auto s10 = l.get_position_at(5000);
    REQUIRE(s10.x == 35);
    REQUIRE(s10.y == 45);
    REQUIRE(s10.z == 350);
    REQUIRE(s10.e == 3500);
}

TEST_CASE("probe_position_lookback_buffer_wraparound") {
    ProbePositionLookback l;

    // add many samples
    for (uint32_t i = 0; i < 1000; i++) {
        l.add_sample(i, i, 0, 0, 0);

        l.current_sample = {
            .time = i + 1,
            .x = 999,
            .y = 0,
            .z = 0,
            .e = 0,
        };

        // check that we can always get last 16 samples back correctly
        for (uint32_t j = 0; j < 16; j++) {
            if (j > i) {
                break;
            }
            uint32_t val = i - j;
            REQUIRE(l.get_position_at(val).x == (float)val);
        }
        // and check that current sample is also correct
        REQUIRE(l.get_position_at(l.current_sample.time).x == 999.0f);

        // and check that 17th sample is not there anymore
        REQUIRE(isnan(l.get_position_at(i - 17).x));
    }
}

TEST_CASE("probe_position_lookback_timer_wraparound") {
    ProbePositionLookback l;

    constexpr uint32_t max = std::numeric_limits<uint32_t>::max();
    constexpr uint32_t step = 1000;
    constexpr uint32_t start = max - step / 2;
    constexpr uint32_t stop = max + step / 2;

    l.add_sample(start - step, 100, 0, 0, 0);
    l.add_sample(start, 200, 0, 0, 0);
    l.add_sample(stop, 300, 0, 0, 0);
    l.add_sample(stop + step, 400, 0, 0, 0);

    l.current_sample = {
        .time = stop + step * 2,
        .x = 500,
        .y = 0,
        .z = 0,
        .e = 0,
    };

    REQUIRE(l.get_position_at(start - step).x == 100);
    REQUIRE(l.get_position_at(start).x == 200);
    REQUIRE(l.get_position_at(start + 100).x == 210);
    REQUIRE(l.get_position_at(start + 100).x == 210);
    REQUIRE(l.get_position_at(start + 500).x == 250);
    REQUIRE(l.get_position_at(start + 900).x == 290);
    REQUIRE(l.get_position_at(stop).x == 300);
    REQUIRE(l.get_position_at(stop + step).x == 400);
    REQUIRE(l.get_position_at(stop + step * 2).x == 500);
}

TEST_CASE("probe_position_nan_return_test") {
    // test case that was actually failing in XL, problem was time difference was too high
    ProbePositionLookback l;

    l.add_sample(32569660, 0, 0, 0, 0);
    l.add_sample(32571686, 0, 0, 0, 0);
    l.add_sample(32573714, 0, 0, 0, 0);
    l.add_sample(32575738, 0, 0, 0, 0);
    l.add_sample(32577764, 0, 0, 0, 0);
    l.add_sample(32579790, 0, 0, 0, 0);
    l.add_sample(32581815, 0, 0, 0, 0);
    l.add_sample(32583842, 0, 0, 0, 0);
    l.add_sample(32585867, 0, 0, 0, 0);
    l.add_sample(32555480, 0, 0, 0, 0);
    l.add_sample(32557505, 0, 0, 0, 0);
    l.add_sample(32559551, 0, 0, 0, 0);
    l.add_sample(32561557, 0, 0, 0, 0);
    l.add_sample(32563583, 0, 0, 0, 0);
    l.add_sample(32565610, 0, 0, 0, 0);
    l.add_sample(32567635, 0, 0, 0, 0);

    l.current_sample = {
        .time = 32580874,
        .x = 0,
        .y = 0,
        .z = 0,
        .e = 0,
    };

    REQUIRE(l.get_position_at(32580874).x == 0);
}
