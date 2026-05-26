#pragma once

#include <cstdint>
#include "selftest_dock_type.hpp"

namespace selftest {
struct DockConfig_t {
    using type_evaluation = SelftestDock_t;
    static constexpr SelftestParts part_type = SelftestParts::Dock;
    PhysicalToolIndex dock_id = PhysicalToolIndex::from_raw(0);
    float z_extra_pos; ///< Raise Z by at least this amount before calibration [mm]
    feedRate_t z_extra_pos_fr; ///< Use this feedrate [mm/s]
};

}; // namespace selftest
