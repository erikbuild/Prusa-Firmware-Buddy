#include "screen_menu_selftest_snake.hpp"
#include <img_resources.hpp>
#include <marlin_client.hpp>
#include <ScreenHandler.hpp>
#include <selftest_types.hpp>
#include <raii/auto_restore.hpp>
#include <option/has_phase_stepping_selftest.h>
#include <option/has_door_sensor_calibration.h>
#include <option/has_indx.h>
#include <string_builder.hpp>
#include <option/has_toolchanger.h>
#include <option/has_manual_belt_tuning.h>
#include <option/has_loadcell.h>
#include <option/has_nozzle_cleaner.h>
#include <option/has_gearbox_alignment.h>
#include <option/has_input_shaper_calibration.h>
#include <option/has_selftest_dependencies.h>
#include <printers.h>
#include <bsod/bsod.h>
#include <option/has_side_fsensor_remap.h>
#include <window_msgbox_happy_printing.hpp>
#if HAS_TOOLCHANGER()
    #include <module/prusa/toolchanger.h>
#endif
#include "queue.h"
#include "Marlin/src/gcode/queue.h"
#include "selftest/i_selftest.hpp"
#include <selftest/selftest_invocation.hpp>
#include <gui/wizard/screen_selftest_submenu.hpp>

#include <option/has_side_fsensor_remap.h>
#if HAS_SIDE_FSENSOR_REMAP()
    #include <feature/filament_sensor/filament_sensors_handler_remap.hpp>
#endif

using namespace SelftestSnake;

namespace SelftestSnake {

inline bool is_multitool() {
#if HAS_TOOLCHANGER()
    return prusa_toolchanger.is_toolchanger_enabled();
#else
    return false;
#endif
}

Action _get_valid_action(Action start_action, int step) {
    assert(step == 1 || step == -1); // other values would cause weird behaviour (endless loop / go beyond array)
    if (is_multitool()) {
        while (is_singletool_only_action(start_action)) {
            start_action = static_cast<Action>(std::to_underlying(start_action) + step);
        }
    } else { // singletool
        while (is_multitool_only_action(start_action)) {
            start_action = static_cast<Action>(std::to_underlying(start_action) + step);
        }
    }
    return start_action;
}

Action get_first_action() {
    return _get_valid_action(Action::_first, 1);
}

Action get_last_action() {
    return _get_valid_action(Action::_last, -1);
}

// Can't (shouldn't) be called with last action
Action get_next_action(Action action) {
    assert(get_last_action() != action && "Unhandled edge case");
    return _get_valid_action(static_cast<Action>(std::to_underlying(action) + 1), 1);
}

bool is_completed(TestResult test_result) {
    // Skipped is also considered completed - it marks non-obligatory tests that have been explicitly skipped by the user
    return test_result == TestResult::passed || test_result == TestResult::skipped;
}

const char *get_action_label(Action action) {
    switch (action) {
    case Action::Fans:
        return N_("Fan Test");
    case Action::ZCheck:
        return N_("Z Axis Test");
    case Action::Heaters:
        return N_("Heater Test");
    case Action::FilamentSensorCalibration:
        return N_("Filament Sensor Calibration");
#if !PRINTER_IS_PRUSA_MINI()
    case Action::ZAlign:
        return N_("Z Alignment Calibration");
#endif
#if PRINTER_IS_PRUSA_MINI() || PRINTER_IS_PRUSA_MK3_5() || PRINTER_IS_PRUSA_MK4()
    case Action::XYCheck:
        return N_("XY Axis Test");
#else
    case Action::XCheck:
        return N_("X Axis Test");
    case Action::YCheck:
        return N_("Y Axis Test");
#endif
#if PRINTER_IS_PRUSA_MINI() || PRINTER_IS_PRUSA_MK3_5()
    case Action::FirstLayer:
        return N_("First Layer Calibration");
#endif
#if HAS_PRECISE_HOMING_COREXY()
    case Action::PreciseHoming:
        return N_("Homing Calibration");
#endif
#if HAS_DOOR_SENSOR_CALIBRATION()
    case Action::DoorSensor:
        return N_("Door Sensor");
#endif
#if HAS_LOADCELL()
    case Action::Loadcell:
        return N_("Loadcell Test");
#endif
#if HAS_GEARBOX_ALIGNMENT()
    case Action::Gears:
        return N_("Gearbox Alignment");
#endif
#if HAS_PHASE_STEPPING_SELFTEST()
    case Action::PhaseSteppingCalibration:
        return N_("Phase Stepping Calibration");
#endif
#if HAS_INDX()
    case Action::BeltTuning:
        return N_("Belt Tuning");
#endif
#if HAS_INDX()
    case Action::InputShaper:
        return N_("Input Shaper Calibration");
#endif
#if HAS_TOOLCHANGER()
    case Action::DockCalibration:
    #if HAS_INDX()
        return N_("Dock Calibration");
    #else
        return N_("Dock Position Calibration");
    #endif
#endif
#if HAS_INDX()
    case Action::NozzleCleanerCalibration:
        return N_("Nozzle Cleaner Calibration");
#endif
#if PRINTER_IS_PRUSA_XL()
    case Action::ToolOffsetsCalibration:
        return N_("Tool Offset Calibration");
    case Action::BedHeaters:
        return N_("Bed Heater Test");
    case Action::NozzleHeaters:
        return N_("Nozzle Heaters Test");
#endif
    case Action::_count:
        assert(false);
        return "";
    }
    bsod_unreachable();
}

#if HAS_SELFTEST_DEPENDENCIES()

bool are_dependencies_met(Action action) {
    const auto dependencies = get_dependencies(action);
    for (Action dependency = get_first_action(), end = get_last_action(); dependency != end; dependency = get_next_action(dependency)) {
        if (!dependencies.test(dependency)) {
            continue;
        }
        if (!is_completed(get_test_result(dependency, AllTools {}))) {
            return false;
        }
    }
    return true;
}

bool are_all_actions_completed() {
    for (Action action = get_first_action(), end = get_last_action(); action != end; action = get_next_action(action)) {
        if (!is_completed(get_test_result(action, AllTools {}))) {
            return false;
        }
    }
    return true;
}

void show_unmet_dependencies_warning(Action action) {
    constexpr int msg_size = 2 * (sizeof("Complete these calibrations first:") + 4 * sizeof("Filament Sensor Calibration"));
    char msg[msg_size];
    StringBuilder sb(msg);
    sb.append_string_view(_("Complete these calibrations first:"));
    const auto dependencies = get_dependencies(action);
    for (Action dependency = get_first_action(); dependency <= Action::_last; dependency = get_next_action(dependency)) {
        if (!dependencies.test(dependency)) {
            continue;
        }
        if (!is_completed(get_test_result(dependency, AllTools {}))) {
            sb.append_printf("\n- ");
            sb.append_string_view(_(get_action_label(dependency)));
        }
    }
    MsgBoxWarning(string_view_utf8::MakeRAM(msg), Responses_Ok);
}

#else

// Can't (shouldn't) be called with first action
Action get_previous_action(Action action) {
    assert(get_first_action() != action && "Unhandled edge case");
    return _get_valid_action(static_cast<Action>(std::to_underlying(action) - 1), -1);
}

bool are_previous_completed(Action action) {
    for (Action act = action; act > get_first_action();) {
        act = get_previous_action(act);
        if (!is_completed(get_test_result(act, AllTools {}))) {
            return false;
        }
    }

    return true;
}

#endif

PhysicalToolIndex get_last_enabled_tool() {
    auto result = PhysicalToolIndex::from_raw(0);
    for (auto tool : PhysicalToolIndex::all().skip_all_disabled()) {
        result = tool;
    }
    return result;
}

PhysicalToolIndex get_next_tool(PhysicalToolIndex tool) {
    assert(tool != get_last_enabled_tool() && "Unhandled edge case");
    do {
        tool = PhysicalToolIndex::from_raw(tool.to_raw() + 1);
    } while (!tool.is_enabled());
    return tool;
}

const img::Resource *get_icon(Action action, ToolMask mask) {
    switch (get_test_result(action, mask)) {
    case TestResult::passed:
        return &img::ok_color_16x16;
    case TestResult::skipped:
        return &img::ok_16x16;
    case TestResult::unknown:
        return &img::na_color_16x16;
    case TestResult::failed:
        return &img::nok_color_16x16;
    }

    assert(false);
    return &img::error_16x16;
}

struct SnakeConfig {
    enum class AutoContinue {
        /// Ask whether to continue after finishing each test
        ask,

        /// Finish all tests in the submenu, then ask
        submenu,

        /// Continue doing all the tests without asking
        all,
    };

    void reset() {
        *this = {};
        last_action = get_last_action();
    }

    void next(Action action, PhysicalToolIndex tool) {
        in_progress = true;
        last_action = action;
        last_tool = tool;
    }

    bool in_progress { false }; ///< Is snake currently running?
    AutoContinue auto_continue { AutoContinue::ask }; ///< Should we continue running other selftests after finishing the current one?

    Action last_action { Action::_last }; ///< Last action that we'have done
    PhysicalToolIndex last_tool { PhysicalToolIndex::from_raw(0) };
};

} // namespace SelftestSnake

static SnakeConfig snake_config {};

namespace {

void do_snake(Action action, PhysicalToolIndex tool) {
    if (!snake_config.in_progress) {
#if HAS_SELFTEST_DEPENDENCIES()
        if (!are_dependencies_met(action)) {
            show_unmet_dependencies_warning(action);
            snake_config.reset();
            return;
        }
#else
        if (!are_previous_completed(action)) {
            if (MsgBoxQuestion(_("Previous Calibrations & Tests are not all done. Continue anyway?"), Responses_YesNo, 1) == Response::No) {
                snake_config.reset();
                return;
            }
        }
#endif
    }

    // Reset invocation state so continue_snake sees aborts only from THIS test, not a previous one.
    selftest_invocation::begin();

    // Note: "gcode" tests are handled separately, partly because
    //       there are not enough bits in the selftest mask.
    {
        bool has_test_special_handling = true;

        switch (action) {

#if HAS_PHASE_STEPPING_SELFTEST()
        case Action::PhaseSteppingCalibration:
            marlin_client::gcode("M1977");
            break;
#endif
        case Action::Fans:
            marlin_client::gcode("M1978");
            break;

        case Action::FilamentSensorCalibration:
#if HAS_SIDE_FSENSOR_REMAP()
            if (!snake_config.in_progress || tool == PhysicalToolIndex::from_raw(0)) {
                // Ask user whether to remap filament sensors
                side_fsensor_remap::ask_to_remap();
            }
#endif
#if HAS_INDX()
            // INDX has no submenu — calibrate all enabled tools in series
            {
                uint8_t mask = 0;
                for (auto t : PhysicalToolIndex::all().skip_all_disabled()) {
                    mask |= (1 << t.to_raw());
                }
                marlin_client::gcode_printf("M1981 F%d", mask);
            }
#else
            marlin_client::gcode_printf("M1981 T%d", tool.to_raw());
#endif
            break;

#if HAS_GEARBOX_ALIGNMENT()
        case Action::Gears:
            marlin_client::gcode_printf("M1979 T%d", tool.to_raw());
            break;
#endif
#if HAS_DOOR_SENSOR_CALIBRATION()
        case Action::DoorSensor:
            marlin_client::gcode("M1980");
            break;
#endif
#if HAS_PRECISE_HOMING_COREXY()
        case Action::PreciseHoming:
            marlin_client::gcode("G28 XY C");
            break;
#endif
#if HAS_INDX()
        case Action::DockCalibration:
            marlin_client::gcode("M1982");
            break;
        case Action::NozzleCleanerCalibration:
            marlin_client::gcode("M1983");
            break;
        case Action::BeltTuning:
            marlin_client::gcode("M961");
            break;
        case Action::InputShaper:
            marlin_client::gcode("M1959");
            break;
#endif
        default:
            has_test_special_handling = false;
            break;
        }

        if (has_test_special_handling) {
            marlin_client::gcode("M118 nop"); // No operation gcode to fill the queue until selftest is done
            snake_config.next(action, tool);
            return;
        }
    }

    if (has_submenu(action)) {
        marlin_client::test_start_with_data(get_test_mask(action), tool);
    } else {
        marlin_client::test_start(get_test_mask(action));
    }

    snake_config.next(action, tool);
};

void continue_snake() {
    const TestResult last_test_result = get_test_result(snake_config.last_action, snake_config.last_tool);
    if (!is_completed(last_test_result)
        || selftest_invocation::is_aborted()) { // last selftest didn't pass
        snake_config.reset();
        return;
    }

    // if the last action was the last action possible
    if (snake_config.last_action == get_last_action()
        && (!has_submenu(get_last_action()) || snake_config.last_tool == get_last_enabled_tool())) {
        snake_config.reset();
        return;
    }

    if (snake_config.auto_continue == SnakeConfig::AutoContinue::submenu && has_submenu(snake_config.last_action) && snake_config.last_tool == get_last_enabled_tool()) {
        snake_config.auto_continue = SnakeConfig::AutoContinue::ask;
    }

    if (snake_config.auto_continue == SnakeConfig::AutoContinue::ask) {
        if (is_multitool() && has_submenu(snake_config.last_action) && snake_config.last_tool != get_last_enabled_tool()) {
            const auto resp = MsgBoxQuestion(_("FINISH remaining calibrations without proceeding to other tests, or perform ALL Calibrations and Tests?\n\nIf you QUIT, all data up to this point is saved."), { Response::Finish, Response::All, Response::Quit }, 2);
            switch (resp) {

            case Response::Finish:
                snake_config.auto_continue = SnakeConfig::AutoContinue::submenu;
                break;

            case Response::All:
                snake_config.auto_continue = SnakeConfig::AutoContinue::all;
                break;

            case Response::Quit:
                snake_config.reset();
                return;

            default:
                bsod_unreachable();
            }

        } else {
            const auto resp = MsgBoxQuestion(_("Continue running Calibrations & Tests?"), { Response::Continue, Response::All, Response::Quit }, 2);
            switch (resp) {

            case Response::Continue:
                snake_config.auto_continue = SnakeConfig::AutoContinue::ask;
                break;

            case Response::All:
                snake_config.auto_continue = SnakeConfig::AutoContinue::all;
                break;

            case Response::Quit:
                snake_config.reset();
                return;

            default:
                bsod_unreachable();
            }
        }
    }

    if (!is_multitool()
        || !has_submenu(snake_config.last_action)
        || snake_config.last_tool == get_last_enabled_tool()) { // singletool or wasn't submenu or was last in a submenu
        do_snake(get_next_action(snake_config.last_action), PhysicalToolIndex::from_raw(0));
    } else { // current submenu not yet finished
        do_snake(snake_config.last_action, get_next_tool(snake_config.last_tool));
    }
}

is_hidden_t get_mainitem_hidden_state(Action action) {
    if constexpr (!option::has_toolchanger) {
        if (requires_toolchanger(action)) {
            return is_hidden_t::yes;
        }
    }

    if ((is_multitool() && is_singletool_only_action(action))
        || (!is_multitool() && is_multitool_only_action(action))) {
        return is_hidden_t::yes;
    } else {
        return is_hidden_t::no;
    }
}

expands_t get_expands(Action action) {
    if (!is_multitool()) {
        return expands_t::no;
    }
    return has_submenu(action) ? expands_t::yes : expands_t::no;
}

constexpr IWindowMenuItem::ColorScheme not_yet_ready_scheme {
    .text { .focused = GuiDefaults::MenuColorBack, .unfocused = GuiDefaults::MenuColorDisabled },
    .back { .focused = GuiDefaults::MenuColorDisabled, .unfocused = GuiDefaults::MenuColorBack },
    .rop {
        .focused { is_inverted::no, has_swapped_bw::no, is_shadowed::no, is_desaturated::no },
        .unfocused { is_inverted::no, has_swapped_bw::no, is_shadowed::no, is_desaturated::no } }
};

} // namespace

// returns the parameter, filled
string_view_utf8 I_MI_STS::get_filled_menu_item_label(Action action) {
    // holds menu indices, indexed by Action
    static const std::array<size_t, std::to_underlying(Action::_count)> action_indices {
        []() {
            std::array<size_t, std::to_underlying(Action::_count)> indices { { {} } };

            int idx { 1 }; // start number
            for (Action act = get_first_action();; act = get_next_action(act)) {
                indices[std::to_underlying(act)] = idx++;
                if (act == get_last_action()) { // explicitly done this way to avoid getting next action of the last action
                    break;
                }
            }
            return indices;
        }()
    };

    char name[max_label_len];
    _(get_action_label(action)).copyToRAM(name, max_label_len);
    snprintf(label_buffer, max_label_len, "%d %s", (int)action_indices[std::to_underlying(action)], name);

    return string_view_utf8::MakeRAM(label_buffer);
}

I_MI_STS::I_MI_STS(Action action)
    : IWindowMenuItem(get_filled_menu_item_label(action), nullptr, is_enabled_t::yes, get_mainitem_hidden_state(action), get_expands(action))
    , action(action) {
#if HAS_SELFTEST_DEPENDENCIES()
    if (!are_dependencies_met(action)) {
#else
    if (!are_previous_completed(action)) {
#endif
        set_color_scheme(&not_yet_ready_scheme);
    }
}

void I_MI_STS::click(IWindowMenu &) {
    if (!has_submenu(action) || !is_multitool()) {
        do_snake(action, PhysicalToolIndex::from_raw(0));
    } else {
        Screens::Access()->Open(ScreenFactory::ScreenWithArg<ScreenSelftestSubmenu>(action));
    }
}

void I_MI_STS::Loop() {
    SetIconId(get_icon(action, AllTools {}));
    set_icon_position(IconPosition::before_extension);
}

I_MI_STS_SUBMENU::I_MI_STS_SUBMENU(const char *label_template, Action action, PhysicalToolIndex tool)
    : IWindowMenuItem(string_view_utf8 {}, nullptr, is_enabled_t::yes, tool.is_enabled() ? is_hidden_t::no : is_hidden_t::yes)
    , action(action)
    , tool(tool) {
    SetLabel(_(label_template).formatted(label_params_, tool.display_index()));
    set_icon_position(IconPosition::before_extension);
}

void I_MI_STS_SUBMENU::click(IWindowMenu &) {
    do_snake(action, tool);
}

void I_MI_STS_SUBMENU::Loop() {
    SetIconId(get_icon(action, tool));
}

namespace SelftestSnake {
void do_menu_event(window_t *receiver, [[maybe_unused]] window_t *sender, GUI_event_t event, [[maybe_unused]] void *param, Action action, bool is_submenu) {
    if (receiver->GetFirstDialog() || event != GUI_event_t::LOOP || !snake_config.in_progress || SelftestInstance().IsInProgress() || marlin_vars().is_processing.get()) {
        // G-code selftests may take a few ticks to execute, do not continue snake while gcode is still in the queue or in progress (no operation gcode is enqueued behind it)
        return;
    }

    // snake is in progress and previous selftest is done
    continue_snake();

    if (!snake_config.in_progress) { // force redraw of current snake menu
        Screens::Access()->Get()->Invalidate();
    }

    if (is_submenu) {
        if (snake_config.last_action == action && snake_config.last_tool == get_last_enabled_tool()) { // finished testing this submenu
            Screens::Access()->Close();
        }
    }
}

bool is_menu_draw_enabled(window_t *window) {
    return !snake_config.in_progress // don't draw if snake is ongoing
        || window->GetFirstDialog(); // always draw if msgbox is being shown
}
} // namespace SelftestSnake

ScreenMenuSTSCalibrations::ScreenMenuSTSCalibrations()
    : SelftestSnake::detail::ScreenMenuSTSCalibrations(_(label)) {
    ClrMenuTimeoutClose(); // No timeout for snake
}

void ScreenMenuSTSCalibrations::draw() {
    if (SelftestSnake::is_menu_draw_enabled(this)) {
        window_frame_t::draw();
    }
}

void ScreenMenuSTSCalibrations::windowEvent(window_t *sender, GUI_event_t event, void *param) {
    do_menu_event(this, sender, event, param, get_first_action(), false);
}

ScreenMenuSTSWizard::ScreenMenuSTSWizard()
    : SelftestSnake::detail::ScreenMenuSTSWizard(_(label)) {
    header.SetIcon(&img::wizard_16x16);
    ClrMenuTimeoutClose(); // No timeout for wizard's snake
}

void ScreenMenuSTSWizard::draw() {
    if ((draw_enabled && !snake_config.in_progress) // don't draw if starting/ending or snake in progress
        || GetFirstDialog() // Always draw when there is a dialog shown
    ) {
        window_frame_t::draw();
    }
}

void ScreenMenuSTSWizard::windowEvent(window_t *sender, GUI_event_t event, void *param) {
    if (event != GUI_event_t::LOOP || GetFirstDialog()) {
        return;
    }

    static bool ever_shown_wizard_box { false };
    if (!ever_shown_wizard_box) {
        ever_shown_wizard_box = true;

        if (MsgBoxPepaCentered(_("Run selftests and calibrations now?"), { Response::Yes, Response::No }) != Response::Yes) {
            Screens::Access()->Close();
            return;
        }

        // Now show always, bed heater selftest can fail if there is no sheet on the bed
        MsgBoxInfo(_("Before you continue, make sure the print sheet is installed on the heatbed."), Responses_Ok);

        do_snake(get_first_action(), PhysicalToolIndex::from_raw(0));
        snake_config.auto_continue = SnakeConfig::AutoContinue::all;
        return;
    }

    do_menu_event(this, sender, event, param, get_first_action(), false);

    if (snake_config.in_progress) {
        draw_enabled = false;
    } else {
        draw_enabled = true;
    }

#if HAS_SELFTEST_DEPENDENCIES()
    if (are_all_actions_completed()) {
#else
    if (is_completed(get_test_result(get_last_action(), AllTools {})) && are_previous_completed(get_last_action())) {
#endif
        MsgBoxHappyPrinting();
        Screens::Access()->Close();
    }
}
