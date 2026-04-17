#pragma once

#include <core/types.h>
#include <variant>

#include <option/has_nozzle_cleaner.h>
#include <option/has_wastebin.h>
#include <option/has_indx.h>
#include <bsod/bsod.h>

namespace mapi {

enum class ZAction : uint16_t {
    move_to_at_least,
    absolute_move,
    relative_move,
    relative_move_skip_xy, // TODO not implemented
    no_move,
    _last = no_move
};

enum class ParkPosition : uint8_t {
    park,
    purge,
    load,
    loadcell_selftest,
    _cnt,
};

/*
This variant version of xyz_pos_t tries to solve the problem of xyz_t having overloaded meaning,
1. absolute position (position in a physical sense) - cannot be NaN
2. position to change to (can be NaN -> it means no change on that specific axis)

This class serves as representation of a position with higher level of abstraction,
it forces you to handle the ParkingPosition::Unchanged value and deal with the real meaning of this class.

It's intended use it to produce xyz_pos_t instance at the end of it's lifetime, having handled ParkingPosition::Unchanged values before that.
*/
struct ParkingPosition {
    // special marker indicating "leave the synchronized xyz_pos_t on that axis as is"
    struct Unchanged {
        constexpr auto operator<=>(const Unchanged &) const = default;
    };
    using Variant = std::variant<Unchanged, float>;

    static constexpr Variant unchanged = Unchanged {};
    Variant x, y, z;

    constexpr auto operator<=>(const ParkingPosition &) const = default;

    // Synchronizes this provided position and provides appropriate xyz_pos_t
    xyz_pos_t to_xyz_pos(const xyz_pos_t &pos) const;

    // Do not use if not necessary! This method currently works as a
    // bridge between unrefactored parts still using xyz_pos_t
    // Should not be needed upon more refactoring
    xyz_pos_t to_nan_xyz_pos() const;

    static ParkingPosition from_xyz_pos(const xyz_pos_t &pos);

    // Provide array-like access if needed
    inline Variant &operator[](size_t index) {
        switch (index) {
        case 0:
            return x;
        case 1:
            return y;
        case 2:
            return z;
        default:
            bsod_unreachable();
        }
    }

    bool operator==(const ParkingPosition &) const = default;
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
 * Does nothing if X or Y axes are not homed.
 *
 * @param z_action Specifies how to handle Z axis movement before XY parking:
 *        - @c move_to_at_least: Raise Z to at least the parking Z height (won't lower)
 *        - @c absolute_move: Move Z to the exact parking Z height
 *        - @c relative_move: Raise Z by the parking Z value (clamped to Z_MAX_POS)
 *        - @c no_move: Skip Z movement, only park XY
 * @param parking_position Target position for parking. Axes set to @c Unchanged
 *        will retain their current position. Defaults to the standard park position.
 */
void park(ZAction z_action, const ParkingPosition &parking_position = get_parking_position(ParkPosition::park));

/**
 * @brief Homes required axes if needed, then parks the toolhead.
 *
 * Same as park(), but performs homing first on axes that will be moved
 * (based on @p parking_position and @p z_action). Z is only homed if
 * @p z_action is @c absolute_move.
 *
 * @param z_action @see park()
 * @param parking_position @see park()
 */
void home_if_needed_and_park(ZAction z_action, const ParkingPosition &parking_position = get_parking_position(ParkPosition::park));

} // namespace mapi
