#pragma once

#include <core/macros.h>
#include <core/types.h>

inline struct Planner {
    static constexpr bool leveling_active = true;

    static inline float z_fade_height = 0;
    static inline float inverse_z_fade_height = 0;

    static void set_z_fade_height(const float zfh) {
        z_fade_height = zfh > 0 ? zfh : 0;
        inverse_z_fade_height = RECIPROCAL(z_fade_height);
    }
} planner;

inline struct UBL {

    static inline float z_correction = 0;

    static inline float get_z_correction(const xyz_pos_t &) {
        return z_correction;
    }

} ubl;
