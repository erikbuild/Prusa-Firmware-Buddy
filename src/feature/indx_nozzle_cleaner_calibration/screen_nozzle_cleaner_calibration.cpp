#include "screen_nozzle_cleaner_calibration.hpp"

#include "indx_nozzle_cleaner_calibration.hpp"
#include <i18n.h>
#include <guiconfig/wizard_config.hpp>
#include <guiconfig/GuiDefaults.hpp>
#include <standard_frame/frame_prompt.hpp>
#include <standard_frame/frame_title_text_image_prompt.hpp>
#include <standard_frame/frame_text_prompt.hpp>
#include <standard_frame/frame_wait.hpp>
#include <common/fsm_base_types.hpp>
#include <img_resources.hpp>
#include <string_view_utf8.hpp>
#include <gui/auto_layout.hpp>

namespace {

constexpr auto txt_title_intro = N_("Nozzle Cleaner Calibration");
constexpr auto txt_intro = N_("You will need to manually position the head at the nozzle cleaner and adjust the height screw.");
constexpr auto txt_picking_tool = N_("Picking tool\n\nPlease wait...");
constexpr auto txt_homing = N_("Homing XY axes\n\nPlease wait...");
constexpr auto txt_moving_away = N_("Raising Z axis for clearance\n\nPlease wait...");
constexpr auto txt_title_move_to_z_point = N_("Nozzle Z-Axis Alignment");
constexpr auto txt_move_to_z_point = N_("Move the head above the nozzle cleaner as shown on the picture.\n\nRotate the adjustment screw until the nozzle is aligned with the cleaner in the Z axis.");
constexpr auto txt_title_ask_position_x = N_("Nozzle X-Axis Alignment");
constexpr auto txt_title_ask_position_y = N_("Nozzle Y-Axis Alignment");
constexpr auto txt_lock_position_x = N_("Motors are now locked.\n\nEnsure your hands are outside the printer enclosure.\n\nCheck that the head is at the correct X position, then press Continue to start measuring.");
constexpr auto txt_lock_position_y = N_("Motors are now locked.\n\nEnsure your hands are outside the printer enclosure.\n\nCheck that the head is at the correct Y position, then press Continue to start measuring.");
constexpr auto txt_ask_position_x = N_("Move the head precisely to the nozzle cleaner X-axis calibration indent in front of the bin.\n\nWhen in position, press Continue.");
constexpr auto txt_ask_position_y = N_("Move the head precisely to the nozzle cleaner Y-axis calibration indent on the side of the bin.\n\nWhen in position, press Continue.");
constexpr auto txt_measuring_x = N_("Measuring X position\n\nDo not touch the printer.");
constexpr auto txt_measuring_y = N_("Measuring Y position\n\nDo not touch the printer.");
constexpr auto txt_success = N_("Nozzle cleaner position has been successfully calibrated and saved.");

constexpr auto txt_evaluating_failed = N_("Calibration failed.\n\nNominal: %.1f mm (+/- %hu mm)\n\nMeasured offset: %.2f mm");
constexpr uint8_t max_offset_mm = 3;

/// Frame for evaluating phases - shows measured offset, nominal value and valid range on failure
class FrameEvaluating {
public:
    FrameEvaluating(window_frame_t *parent, FSMAndPhase fsm_phase, const string_view_utf8 &)
        : info(parent, {}, is_multiline::yes, is_closed_on_click_t::no, string_view_utf8::MakeNULLSTR())
        , radio(parent, {}, fsm_phase) {

        info.SetAlignment(Align_t::LeftCenter());

        parent->CaptureNormalWindow(radio);

        static constexpr std::array layout {
            StackLayoutItem { .height = StackLayoutItem::stretch, .margin_side = 16, .margin_top = 16 },
            standard_stack_layout::for_radio,
        };
        std::array<window_t *, layout.size()> windows { &info, &radio };
        layout_vertical_stack(parent->GetRect(), windows, layout);
    }

    void update(fsm::PhaseData data) {
        const auto eval_data = fsm::deserialize_data<indx_nozzle_cleaner_calibration::EvaluatingData>(data);
        info.SetText(_(txt_evaluating_failed).formatted(params, static_cast<double>(eval_data.nominal()), max_offset_mm, static_cast<double>(eval_data.offset())));
    }

private:
    window_text_t info;
    RadioButtonFSM radio;
    StringViewUtf8Parameters<60> params;
};

static constexpr const img::Resource *img_move_to_z_point = &img::cleaner_calibration_z;
static constexpr const img::Resource *img_ask_position_y = &img::cleaner_calibration_y;
static constexpr const img::Resource *img_ask_position_x = &img::cleaner_calibration_x;

using Frames = FrameDefinitionList<ScreenNozzleCleanerCalibration::FrameStorage,
    FrameDefinition<PhaseNozzleCleanerCalibration::intro, FramePrompt, PhaseNozzleCleanerCalibration::intro, txt_title_intro, txt_intro>,
    FrameDefinition<PhaseNozzleCleanerCalibration::picking_tool, FrameWait, txt_picking_tool>,
    FrameDefinition<PhaseNozzleCleanerCalibration::homing, FrameWait, txt_homing>,
    FrameDefinition<PhaseNozzleCleanerCalibration::moving_away, FrameWait, txt_moving_away>,
    FrameDefinition<PhaseNozzleCleanerCalibration::move_to_z_point, FrameTitleTextImagePrompt, PhaseNozzleCleanerCalibration::move_to_z_point, txt_title_move_to_z_point, txt_move_to_z_point, img_move_to_z_point>,
    FrameDefinition<PhaseNozzleCleanerCalibration::ask_position_y, FrameTitleTextImagePrompt, PhaseNozzleCleanerCalibration::ask_position_y, txt_title_ask_position_y, txt_ask_position_y, img_ask_position_y>,
    FrameDefinition<PhaseNozzleCleanerCalibration::lock_position_y, FrameTextPrompt, PhaseNozzleCleanerCalibration::lock_position_y, txt_lock_position_y>,
    FrameDefinition<PhaseNozzleCleanerCalibration::measuring_y, FrameWait, txt_measuring_y>,
    FrameDefinition<PhaseNozzleCleanerCalibration::evaluating_y, FrameEvaluating, PhaseNozzleCleanerCalibration::evaluating_y, txt_evaluating_failed>,
    FrameDefinition<PhaseNozzleCleanerCalibration::ask_position_x, FrameTitleTextImagePrompt, PhaseNozzleCleanerCalibration::ask_position_x, txt_title_ask_position_x, txt_ask_position_x, img_ask_position_x>,
    FrameDefinition<PhaseNozzleCleanerCalibration::lock_position_x, FrameTextPrompt, PhaseNozzleCleanerCalibration::lock_position_x, txt_lock_position_x>,
    FrameDefinition<PhaseNozzleCleanerCalibration::measuring_x, FrameWait, txt_measuring_x>,
    FrameDefinition<PhaseNozzleCleanerCalibration::evaluating_x, FrameEvaluating, PhaseNozzleCleanerCalibration::evaluating_x, txt_evaluating_failed>,
    FrameDefinition<PhaseNozzleCleanerCalibration::calibration_success, FrameTextPrompt, PhaseNozzleCleanerCalibration::calibration_success, txt_success>>;

} // namespace

ScreenNozzleCleanerCalibration::ScreenNozzleCleanerCalibration()
    : ScreenFSM { N_("NOZZLE CLEANER CALIBRATION"), GuiDefaults::RectScreenNoHeader } {
    CaptureNormalWindow(inner_frame);
    create_frame();
}

ScreenNozzleCleanerCalibration::~ScreenNozzleCleanerCalibration() {
    destroy_frame();
}

void ScreenNozzleCleanerCalibration::create_frame() {
    Frames::create_frame(frame_storage, get_phase(), &inner_frame);
}

void ScreenNozzleCleanerCalibration::destroy_frame() {
    Frames::destroy_frame(frame_storage, get_phase());
}

void ScreenNozzleCleanerCalibration::update_frame() {
    Frames::update_frame(frame_storage, get_phase(), fsm_base_data.GetData());
}
