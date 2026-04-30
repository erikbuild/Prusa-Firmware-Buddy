/// @file
#pragma once

#include "base_hotend.hpp"
#include <tool/tool/standard_fff_physical_tool.hpp>

/// Represents a hotend that is managed by a dwarf on an XL
class IndxHotend final : public BaseHotend {
    friend class PrusaToolChanger; // For access to start_heating / stop_heating
    friend class PrusaToolChangerUtils;

public:
    /// !!! Careful, the config pointer is stored, so make sure the config is persistent!
    explicit IndxHotend(PhysicalToolIndex tool, const Config *config)
        : BaseHotend(tool, config) {
        is_thermally_managed_ = false; // INDX starts inactive
    }

    static StandardFFFPhysicalTool<IndxHotend> &indx_tool(PhysicalToolIndex tool);

    /// Asserts the toolchanger thermal-management invariant: no tool other than
    /// `expected_managed` may currently be thermally managed. Pass NoTool to assert
    /// that no tool is managed at all.
    static void assert_thermally_managed_invariant(std::variant<PhysicalToolIndex, NoTool> expected_managed);

protected:
    virtual void manage() override;

    /// Pushes the current target to the puppy and resets the hotend temp compensator.
    /// Engages other heating side effects common to all hotend implementations.
    /// changes (via BaseHotend::set_nozzle_target_temp) and on pickup (via start_heating).
    void handle_nozzle_target_change() override;

private:
    /// Fires heating side effects (safety_timer wake, power_on, watch reset, puppy push,
    /// temp model reset) and arms protection state machines.
    /// !!! Toolchanger-only: use at pickup, when transitioning into active heating. !!!
    void start_heating();

    /// Stop heating physically. Logical target is preserved so a future pickup can resume.
    /// !!! Toolchanger-only: use at park, open_head, recovery — not as a way to set target=0. !!!
    void stop_heating();
};
