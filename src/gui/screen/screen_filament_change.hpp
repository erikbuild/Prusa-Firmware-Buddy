#pragma once

#include <screen_fsm.hpp>
#include <fsm/filament_change_phases.hpp>

class ScreenFilamentChange : public ScreenFSM {
public:
    ScreenFilamentChange();
    ~ScreenFilamentChange();

protected:
    void create_frame() override;
    void destroy_frame() override;
    void update_frame() override;

private:
    inline PhasesLoadUnload get_phase() const {
        return GetEnumFromPhaseIndex<PhasesLoadUnload>(fsm_base_data.GetPhase());
    }
};
