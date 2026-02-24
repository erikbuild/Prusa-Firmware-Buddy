/// @file
#include <module/motion.h>
#include <module/planner.h>

#if HAS_LEVELING
    #include <feature/bedlevel/bedlevel.h>
#endif

MachinePosXYZ to_machine_pos(const xyz_pos_t &pos) {
    MachinePosXYZ result = pos.to_tag<MachinePosTag>();

#if HAS_MESH
    if (planner.leveling_active) {
        const float fade_scaling_factor = planner.fade_scaling_factor_for_z(pos.z);
        result.z += fade_scaling_factor * ubl.get_z_correction(result);
    }
#endif

    return result;
}

xyz_pos_t to_native_pos(const MachinePosXYZ &pos) {
    xyz_pos_t result = pos.to_tag<NativePosTag>();

#if HAS_MESH
    if (planner.leveling_active) {
        // !!! This is WRONG. We are taking the scaling factor from the wrong Z.
        // We should be takking it from the original Z, but we can't because we don't know it,
        // otherwise we wouldn't be calling this function to compute it.
        // This logic is principially flawed, I don't have a better solution at hand though.
        const float fade_scaling_factor = planner.fade_scaling_factor_for_z(pos.z);
        result.z -= fade_scaling_factor * ubl.get_z_correction(result);
    }
#endif

    return result;
}
