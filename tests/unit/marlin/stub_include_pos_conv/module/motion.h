#pragma once

#include <core/types.h>

MachinePosXYZ to_machine_pos(const xyz_pos_t &pos);
xyz_pos_t to_native_pos(const MachinePosXYZ &pos);
