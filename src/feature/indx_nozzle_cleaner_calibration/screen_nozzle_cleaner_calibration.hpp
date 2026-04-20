#pragma once

#include <screen_fsm.hpp>

class ScreenNozzleCleanerCalibration final : public ScreenFSM {
public:
    ScreenNozzleCleanerCalibration();
    ~ScreenNozzleCleanerCalibration();

    inline PhaseNozzleCleanerCalibration get_phase() const {
        return GetEnumFromPhaseIndex<PhaseNozzleCleanerCalibration>(fsm_base_data.GetPhase());
    }

protected:
    void create_frame() final;
    void destroy_frame() final;
    void update_frame() final;
};
