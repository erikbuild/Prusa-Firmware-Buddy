#pragma once

#include <core/types.h>
#include <variant>

#include <option/has_nozzle_cleaner.h>
#include <option/has_wastebin.h>
#include <option/has_indx.h>
#include <bsod/bsod.h>

namespace mapi {

enum class ParkPosition : uint8_t {
    park,

    /// Position where it's safe to purge
    /// If the printer has a wastebin, it will be over the wastebin
    /// Otherwise some generic location one can poop from
    purge,

    load,
    unload,
    loadcell_selftest,
    _cnt,
};

/// Describes a position, or rather behavior for parking
/// The behavior might also be for example a relative Z lift, or some axes might not be moved at all
struct ParkingPosition {
    // special marker indicating "leave the synchronized xyz_pos_t on that axis as is"
    struct Unchanged {
        constexpr bool operator==(const Unchanged &) const = default;
    };

    /// Moves relatively to the current position, clamped to the machine limits
    struct Relative {
        float delta;

        constexpr bool operator==(const Relative &) const = default;
    };

    /// Parks the Z axis at the specified minimum distance
    struct Minimum {
        /// A little trick to prevent users from doing Minimum { 5.5f }
        /// Encourages using aggregate initializer Minimum { .above_print = ... }
        [[no_unique_address]] std::monostate _use_aggregatee_initizalizer {};

        /// Don't park lower than this distance above print (planner.max_printed_z) if specified
        float above_print = NAN;

        /// Don't park lower than this absolute position if specified
        float absolute = 0;

        constexpr bool operator==(const Minimum &) const = default;
    };

    static constexpr Unchanged unchanged {};

    using X = std::variant<Unchanged, float>;
    using Y = std::variant<Unchanged, float>;
    using Z = std::variant<Unchanged, float, Relative, Minimum>;

    // float = absolute coordinate
    X x = unchanged;
    Y y = unchanged;
    Z z = unchanged;

    constexpr bool operator==(const ParkingPosition &) const = default;

    /// @returns a vector of which axes need to be homed for the parking to the position to be realizable
    xyz_bool_t axes_needing_homing() const;

    /// Resolves the Z component against reference_z (the current Z): Unchanged yields
    /// reference_z, an absolute value yields itself, Relative offsets reference_z and
    /// Minimum raises it to a floor; Relative/Minimum are clamped to Z_MAX_POS.
    float resolve_z(float reference_z) const;

    // Synchronizes this provided position and provides appropriate xyz_pos_t
    xyz_pos_t to_xyz_pos(const xyz_pos_t &pos) const;

    // Do not use if not necessary! This method currently works as a
    // bridge between unrefactored parts still using xyz_pos_t
    // Should not be needed upon more refactoring
    [[deprecated("Construct the ParkingPosition properly")]]
    xyz_pos_t to_nan_xyz_pos(const xyz_pos_t &pos = { NAN, NAN, NAN }) const;

    [[deprecated("Construct the ParkingPosition properly")]]
    static ParkingPosition from_xyz_pos(const xyz_pos_t &pos);

    [[deprecated("Construct the ParkingPosition properly")]]
    static ParkingPosition from_xy_relative_z_pos(const xyz_pos_t &pos);

    /// @returns a modified parking position with Z not moving at all
    [[deprecated("Construct the ParkingPosition properly")]]
    ParkingPosition without_z_move() const {
        auto result = *this;
        result.z = unchanged;
        return result;
    }
};

ParkingPosition get_parking_position(ParkPosition position);

#if HAS_NOZZLE_CLEANER()
void move_out_of_nozzle_cleaner_area();

    #if HAS_INDX()
/// Applies nozzle cleaner origin offsets (from calibration) to the given parking position's X and Y.
ParkingPosition apply_nozzle_cleaner_offset(const ParkingPosition &position);
    #endif

#endif

/**
 * @brief Parks the toolhead at the specified position.
 *
 * Moves the toolhead to a parking position, optionally adjusting the Z axis first.
 * On printers with a nozzle cleaner, the function automatically performs intermediate
 * moves to avoid collisions with the brush/v-blade area.
 *
 * Can only execute part of parking move if axes are not homed
 *
 * @returns if the whole intended move was executed
 */
bool park(const ParkingPosition &parking_position = get_parking_position(ParkPosition::park));

/**
 * @brief Homes required axes if needed, then parks the toolhead.
 *
 * Same as park(), but performs homing first on axes that will need it
 */
void home_if_needed_and_park(const ParkingPosition &parking_position = get_parking_position(ParkPosition::park));

} // namespace mapi
