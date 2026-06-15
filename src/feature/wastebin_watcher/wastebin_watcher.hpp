/// @file
#pragma once

#include <option/has_wastebin_fill_tracking.h>

#include <atomic>
#include <cstdint>

// Compiled into the build only where the feature is enabled (added via CMake). A static_assert
// (instead of #if-ing the whole header) turns a stray include on a non-feature build into a hard
// error rather than silently nothing.
static_assert(HAS_WASTEBIN_FILL_TRACKING(), "wastebin_watcher.hpp included on a build without HAS_WASTEBIN_FILL_TRACKING()");

/**
 * Centralizes the INDX nozzle-cleaner wastebin fill policy: pellet accounting, mid-print full
 * detection (+ optional auto-pause), pre-print overfill projection and the per-print monitoring
 * state. The persistent pellet count lives in Odometer_s; this is the policy on top of it. Singleton.
 *
 * Thread-safety is marked per method below ("[Marlin thread only]" / "[Thread-safe]").
 */
class WastebinWatcher {
public:
    /// [Thread-safe] Singleton accessor.
    static WastebinWatcher &instance();

    /// [Marlin thread only] G12: account for one ejected blob (== one pellet). Bumps the counters
    /// and, mid-print, raises the "wastebin full" warning (and optionally auto-pauses, via
    /// pause_to_empty()) the moment the bin fills.
    void account_ejected_pellet();

    /// [Marlin thread only] Park the head clear of the bin (retracted + lifted above the print),
    /// raise the wastebin warning and block the gcode stream until the user answers it, then return
    /// and de-retract. Does motion, so it must run on the marlin thread.
    /// \p full true: mid-print "bin full" flow - raises NozzleCleanerFull (Ignore / Done).
    ///         false: manual "Empty Wastebin" flow (M1986) - raises NozzleCleanerManualEmpty (Done).
    /// The warning dispatch handles the response (reset the counter / ignore for this print). Shared
    /// by account_ejected_pellet() (auto-pause) and the M1986 gcode (manual).
    void pause_to_empty(bool full);

    /// [Thread-safe] True if this print is likely to overfill the bin (current fill + this print's
    /// toolchanges exceed capacity). False when the gcode carries no toolchange count (older slicer).
    bool print_will_overfill() const;

    /// [Thread-safe] Reset the per-print toolchange progress. Called when a print starts.
    void reset_print_progress();

    /// [Thread-safe] Suppress (true) / re-enable (false) the mid-print full check for the current print.
    void set_ignored_for_print(bool ignored);

    /// [Thread-safe] The bin was emptied: reset the persistent pellet counter to zero. Called from
    /// the warning dispatch (marlin thread) when the user confirms.
    void mark_emptied();

    /// [Thread-safe] Pellet capacity of the installed cleaner (standard / extended), per the persisted setting.
    uint32_t capacity() const;
    /// [Thread-safe] Pellets in the bin since it was last emptied (current fill, not an action).
    uint32_t fill_level() const;
    /// [Thread-safe] Pellets still expected to be ejected in the rest of this print (one per
    /// remaining toolchange; 0 if the gcode carries no toolchange count).
    uint32_t expected_remaining_pellets() const;

private:
    WastebinWatcher() = default;

    /// Set by the pre-print / mid-print "Ignore": stop checking the bin for the rest of this print.
    std::atomic<bool> ignored_for_print_ = false;
    /// Pellets ejected during the current print (== toolchange progress). Reset at print start.
    std::atomic<uint32_t> pellets_this_print_ = 0;
};
