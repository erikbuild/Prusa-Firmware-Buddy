#include <catch2/catch_test_macros.hpp>
#include <utils/timing/latching_debouncer.hpp>

using namespace utils;

TEST_CASE("LatchingDebouncer", "[latching_debouncer]") {
    // Use threshold = 750 throughout tests.
    const uint32_t threshold = 750;

    SECTION("Fresh instance is not triggered") {
        LatchingDebouncer debouncer;
        CHECK(!debouncer.is_triggered());
    }

    SECTION("Single active sample below threshold") {
        LatchingDebouncer debouncer;
        debouncer.update(true, 100, threshold);
        CHECK(!debouncer.is_triggered());
    }

    SECTION("Threshold crossing") {
        LatchingDebouncer debouncer;

        // Active sample at t=100, start pending.
        debouncer.update(true, 100, threshold);
        CHECK(!debouncer.is_triggered());

        // Still pending at t=849 (elapsed = 749, just below threshold).
        debouncer.update(true, 849, threshold);
        CHECK(!debouncer.is_triggered());

        // Cross threshold at t=850 (elapsed = 750).
        debouncer.update(true, 850, threshold);
        CHECK(debouncer.is_triggered());
    }

    SECTION("Pending cleared by inactive sample") {
        LatchingDebouncer debouncer;

        // Enter pending at t=100.
        debouncer.update(true, 100, threshold);
        CHECK(!debouncer.is_triggered());

        // Inactive at t=500, back to idle.
        debouncer.update(false, 500, threshold);
        CHECK(!debouncer.is_triggered());

        // New active sample at t=700 starts fresh pending.
        debouncer.update(true, 700, threshold);
        CHECK(!debouncer.is_triggered());

        // At t=1449, elapsed from t=700 is 749 (still below 750).
        debouncer.update(true, 1449, threshold);
        CHECK(!debouncer.is_triggered());

        // At t=1450, elapsed from t=700 is 750 (at threshold).
        debouncer.update(true, 1450, threshold);
        CHECK(debouncer.is_triggered());
    }

    SECTION("Triggered cleared and full debounce window re-entered") {
        LatchingDebouncer debouncer;

        // Drive to triggered at t=1000.
        debouncer.update(true, 100, threshold);
        debouncer.update(true, 850, threshold);
        CHECK(debouncer.is_triggered());

        // Inactive at t=1000, back to idle.
        debouncer.update(false, 1000, threshold);
        CHECK(!debouncer.is_triggered());

        // Active at t=1100, enter pending.
        debouncer.update(true, 1100, threshold);
        CHECK(!debouncer.is_triggered());

        // Still pending at t=1849 (elapsed = 749).
        debouncer.update(true, 1849, threshold);
        CHECK(!debouncer.is_triggered());

        // Cross threshold at t=1850 (elapsed = 750).
        debouncer.update(true, 1850, threshold);
        CHECK(debouncer.is_triggered());
    }

    SECTION("Triggered state survives timestamp wraparound") {
        LatchingDebouncer debouncer;

        // Drive to triggered at t=1000.
        debouncer.update(true, 100, threshold);
        debouncer.update(true, 850, threshold);
        CHECK(debouncer.is_triggered());

        // Keep feeding active samples at large timestamps.
        // No inactive in between.
        debouncer.update(true, 0x80000000U, threshold);
        CHECK(debouncer.is_triggered());

        debouncer.update(true, 0xC0000000U, threshold);
        CHECK(debouncer.is_triggered());

        debouncer.update(true, 0xFFFFFFFFU, threshold);
        CHECK(debouncer.is_triggered());

        debouncer.update(true, 0x00000010U, threshold);
        CHECK(debouncer.is_triggered());
    }

    SECTION("Threshold crossing across timestamp wrap while pending") {
        LatchingDebouncer debouncer;

        // Enter pending at pending_since = 0xFFFFFF00.
        debouncer.update(true, 0xFFFFFF00U, threshold);
        CHECK(!debouncer.is_triggered());

        // Just short of threshold across the wrap:
        // 0xFFFFFF00 + 749 mod 2^32 = 0x000001ED.
        debouncer.update(true, 0x000001EDU, threshold);
        CHECK(!debouncer.is_triggered());

        // Exactly at threshold elapsed:
        // 0xFFFFFF00 + 750 mod 2^32 = 0x000001EE.
        debouncer.update(true, 0x000001EEU, threshold);
        CHECK(debouncer.is_triggered());
    }

    SECTION("Inactive sample at exact transition tick") {
        // At the exact threshold tick, inactive should win and clear to idle.
        LatchingDebouncer debouncer;

        // Enter pending at t=100.
        debouncer.update(true, 100, threshold);
        CHECK(!debouncer.is_triggered());

        // At t=850 (elapsed = 750), send inactive instead of active.
        // Inactive wins, should clear to idle.
        debouncer.update(false, 850, threshold);
        CHECK(!debouncer.is_triggered());

        // A subsequent active sample at t=851 starts fresh pending.
        debouncer.update(true, 851, threshold);
        CHECK(!debouncer.is_triggered());

        // Verify we need the full threshold again.
        debouncer.update(true, 1600, threshold);
        CHECK(!debouncer.is_triggered());

        debouncer.update(true, 1601, threshold);
        CHECK(debouncer.is_triggered());
    }
}
