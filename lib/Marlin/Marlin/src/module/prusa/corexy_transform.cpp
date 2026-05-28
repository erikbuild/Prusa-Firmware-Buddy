/*
 * CoreXY kinematic transforms (AB stepper space <-> XY machine space).
 *
 * These are pure CoreXY kinematics.
 * They live in a separate TU so they remain available when
 * homing_corexy.cpp (precise homing refinement) is not compiled in.
 */

#include "corexy_transform.hpp"

#include <module/planner.h>

void corexy_ab_to_xy(const ab_steps_t &steps, MachinePosXY &mm) {
    const float x = static_cast<float>(steps.a + steps.b) / 2.f;
    const float y = static_cast<float>(CORESIGN(steps.a - steps.b)) / 2.f;
    mm.x = x * planner.mm_per_step[X_AXIS];
    mm.y = y * planner.mm_per_step[Y_AXIS];
}

void corexy_ab_to_xyze(const ab_steps_t &steps, MachinePosXYZE &mm) {
    {
        MachinePosXY xy;
        corexy_ab_to_xy(steps, xy);
        mm.set(xy);
    }
    LOOP_S_L_N(i, C_AXIS, XYZE_N) {
        mm[i] = planner.get_axis_position_mm((AxisEnum)i);
    }
}
