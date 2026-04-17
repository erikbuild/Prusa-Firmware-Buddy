#pragma once

#include <screen_fsm.hpp>

class ScreenDockCalibration final : public ScreenFSM {
public:
    ScreenDockCalibration();
    ~ScreenDockCalibration();

    inline PhaseDockCalibration get_phase() const {
        return GetEnumFromPhaseIndex<PhaseDockCalibration>(fsm_base_data.GetPhase());
    }

protected:
    void create_frame() final;
    void destroy_frame() final;
    void update_frame() final;
};
