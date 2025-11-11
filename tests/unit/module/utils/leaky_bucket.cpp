#include <catch2/catch.hpp>
#include <utils/leaky_bucket.hpp>

TEST_CASE("LeakyBucket basic functionality") {
    LeakyBucket bucket;

    SECTION("Ratelimiter") {
        // Degenerated mode in which it fits a single sample - it acts as an
        // ordinary rate limitter.
        bucket.set_parameters(1, 1000);

        // First one fits.
        REQUIRE(bucket.add_sample(0));
        // Second one just after that one doesn't.
        REQUIRE_FALSE(bucket.add_sample(1));
        // Not until the whole second passed.
        REQUIRE_FALSE(bucket.add_sample(999));

        // But then after the whole second is over, we can add one more.
        REQUIRE(bucket.add_sample(1000));

        // When we reset, we can add a new one immediatelly.
        REQUIRE_FALSE(bucket.add_sample(1001));
        bucket.reset();
        REQUIRE(bucket.add_sample(1003));

        // New sample counts from the last sample (not multiples of 1000 from epoch).
        REQUIRE_FALSE(bucket.add_sample(2002));
        REQUIRE(bucket.add_sample(2003));
    }

    SECTION("Multiple samples") {
        // An initial burst is all fine
        bucket.set_parameters(5, 1000);
        for (uint32_t i = 0; i < 5; i++) {
            REQUIRE(bucket.add_sample(i));
        }

        // But another one doesn't fit
        REQUIRE_FALSE(bucket.add_sample(6));

        // At this time, the first one cleared out and we can add one more.
        REQUIRE(bucket.add_sample(1000));
        // But only one
        REQUIRE_FALSE(bucket.add_sample(1000));
    }

    SECTION("Clock overflow") {
        bucket.set_parameters(1, 1000);
        REQUIRE(bucket.add_sample(std::numeric_limits<uint32_t>::max() - 500));
        REQUIRE_FALSE(bucket.add_sample(std::numeric_limits<uint32_t>::max()));
        REQUIRE_FALSE(bucket.add_sample(0));
        REQUIRE(bucket.add_sample(500));
    }

    SECTION("Always full") {
        bucket.set_parameters(0, 1000);
        REQUIRE_FALSE(bucket.add_sample(10000));
    }

    SECTION("Always empty, increased time") {
        bucket.set_parameters(1, 0);
        for (uint32_t i = 0; i < 100; i++) {
            REQUIRE(bucket.add_sample(i));
        }
    }

    SECTION("Always empty, constant time") {
        bucket.set_parameters(1, 0);
        for (uint32_t i = 0; i < 100; i++) {
            REQUIRE(bucket.add_sample(0));
        }
    }
}
