#pragma once

#include <screen_fsm.hpp>
#include <marlin_server_types/fsm/print_preview_phases.hpp>

class ScreenPrintPreview final : public ScreenFSM {

public:
    ScreenPrintPreview();
    ~ScreenPrintPreview();

protected:
    inline PhasesPrintPreview get_phase() const {
        return GetEnumFromPhaseIndex<PhasesPrintPreview>(fsm_base_data.GetPhase());
    }

protected:
    void create_frame() override;
    void destroy_frame() override;
    void update_frame() override;
    virtual void windowEvent(window_t *sender, GUI_event_t event, void *param) override;
};
