#include "selftest_tool_offsets.hpp"
#include "Marlin/src/gcode/queue.h"
#include "Marlin/src/module/stepper.h"
#include "marlin_server.hpp"
#include "selftest_tool_helper.hpp"
#include "Marlin/src/module/temperature.h"
#include "fanctl.hpp"
#include <marlin_stubs/G425.hpp>
#include <tool_index.hpp>
#include <utils/storage/strong_index_array.hpp>

#include <option/has_toolchanger.h>
#if HAS_TOOLCHANGER()
    #include <Marlin/src/module/prusa/toolchanger.h>
#endif

using namespace selftest;
LOG_COMPONENT_REF(Selftest);

namespace {
/// @brief Set temperature to all enabled tools
void set_nozzle_temps(int16_t temp) {
    for (auto tool : PhysicalToolIndex::all()) {
        if (is_tool_selftest_enabled(tool, ToolMask::AllTools)) { // set temperature on all tools, its not possible to calibrate just one tool
            thermalManager.setTargetHotend(temp, tool);
        }
    }
}

/// @brief Check temperature of all enabled tools is at target
bool all_nozzles_at_target() {
    for (auto tool : PhysicalToolIndex::all()) {
        if (is_tool_selftest_enabled(tool, ToolMask::AllTools)) { // check temperature on all tools, its not possible to calibrate just one tool
            if (!thermalManager.is_hotend_temperature_reached(tool)) {
                return false;
            }
        }
    }
    return true;
}
}; // namespace

/// @brief Helper class that turns fans to 100% on when cooldown is needed, and allows to reset fans back to normal control
class FanCoolingManager {
public:
    /// Request cooldown on all tools
    static void cooldown() {
        for (auto tool : PhysicalToolIndex::all()) {
            if (is_tool_selftest_enabled(tool, ToolMask::AllTools) && thermalManager.degHotend(tool) > SelftestToolOffsets_t::TOOL_CALIBRATION_TEMPERATURE && // tool is hot
                !tool_cooling_down[tool]) { // cooling is not already turned on

                start_cooling(tool);
            }
        }
    }

    /// manage cooling down (to be called periodically)

    static void manage() {
        // periodically check if tool is cooled down, stop fans
        for (auto tool : PhysicalToolIndex::all()) {
            if (is_tool_selftest_enabled(tool, ToolMask::AllTools) && // manage temperature on all tools, its not possible to calibrate just one tool
                thermalManager.degHotend(tool) <= SelftestToolOffsets_t::TOOL_CALIBRATION_TEMPERATURE && tool_cooling_down[tool]) {
                stop_cooling(tool);
            }
        }
    }

    /// When cooldown is active, reset it and go back to normal fan operation
    static void reset() {
        for (auto tool : PhysicalToolIndex::all()) {
            if (is_tool_selftest_enabled(tool, ToolMask::AllTools) && // manage temperature on all tools, its not possible to calibrate just one tool
                tool_cooling_down[tool]) { // tool is cooling down

                stop_cooling(tool);
            }
        }
    }

private:
    static StrongIndexArray<bool, PhysicalToolIndex::count, PhysicalToolIndex, PhysicalToolIndex::to_raw_static> tool_cooling_down;

    static void start_cooling(PhysicalToolIndex tool) {
        tool_cooling_down[tool] = true;
        Fans::print(tool).enter_selftest_mode();
        Fans::heat_break(tool).enter_selftest_mode();
        Fans::print(tool).selftest_set_pwm(255);
        Fans::heat_break(tool).selftest_set_pwm(255);
    }

    static void stop_cooling(PhysicalToolIndex tool) {
        tool_cooling_down[tool] = false;
        Fans::print(tool).exit_selftest_mode();
        Fans::heat_break(tool).exit_selftest_mode();
    }
};

StrongIndexArray<bool, PhysicalToolIndex::count, PhysicalToolIndex, PhysicalToolIndex::to_raw_static> FanCoolingManager::tool_cooling_down = { false };

CSelftestPart_ToolOffsets::CSelftestPart_ToolOffsets(IPartHandler &state_machine, const ToolOffsetsConfig_t &config, SelftestToolOffsets_t &result)
    : state_machine(state_machine)
    , result(result)
    , config(config) {}

CSelftestPart_ToolOffsets::~CSelftestPart_ToolOffsets() {
    FanCoolingManager::reset();
}

LoopResult CSelftestPart_ToolOffsets::state_ask_user_confirm_start() {
    IPartHandler::SetFsmPhase(PhasesSelftest::ToolOffsets_wait_user_confirm_start);
    return LoopResult::RunNext;
}

LoopResult CSelftestPart_ToolOffsets::state_clean_nozzle_start() {
    IPartHandler::SetFsmPhase(PhasesSelftest::ToolOffsets_wait_move_away);
    disable_all_steppers(); // Let the user operate tools, pull out the filament if required
    set_nozzle_temps(0);

    return LoopResult::RunNext;
}

LoopResult CSelftestPart_ToolOffsets::state_move_away() {
    IPartHandler::SetFsmPhase(PhasesSelftest::ToolOffsets_wait_user_clean_nozzle_cold);
    // we'll ask user to clean nozzle and put on sheet - so give him some space
    do_z_clearance(100);
    return LoopResult::RunNext;
}

LoopResult CSelftestPart_ToolOffsets::state_clean_nozzle() {
    const auto button_pressed = state_machine.GetButtonPressed();

    if (button_pressed == Response::Continue) {
        set_nozzle_temps(SelftestToolOffsets_t::TOOL_CALIBRATION_TEMPERATURE);
        FanCoolingManager::cooldown();
        return LoopResult::RunNext;
    }

    if (IPartHandler::GetFsmPhase() == PhasesSelftest::ToolOffsets_wait_user_clean_nozzle_hot) {
        // nozzle is hot or heating up
        if (button_pressed == Response::Cooldown) {
            IPartHandler::SetFsmPhase(PhasesSelftest::ToolOffsets_wait_user_clean_nozzle_cold);
            set_nozzle_temps(SelftestToolOffsets_t::TOOL_CALIBRATION_TEMPERATURE);
            FanCoolingManager::cooldown();
        }
    } else if (IPartHandler::GetFsmPhase() == PhasesSelftest::ToolOffsets_wait_user_clean_nozzle_cold) {
        // nozzle is cold or cooling down
        if (button_pressed == Response::Heatup) {
            IPartHandler::SetFsmPhase(PhasesSelftest::ToolOffsets_wait_user_clean_nozzle_hot);
            set_nozzle_temps(SelftestToolOffsets_t::TOOL_OFFSET_CLEANING_TEMPERATURE);
            FanCoolingManager::reset();
        }
    }

    FanCoolingManager::manage();

    return LoopResult::RunCurrent;
}

LoopResult CSelftestPart_ToolOffsets::state_ask_user_install_sheet() {
    IPartHandler::SetFsmPhase(PhasesSelftest::ToolOffsets_wait_user_install_sheet);
    return LoopResult::RunNext;
}

LoopResult CSelftestPart_ToolOffsets::state_wait_user() {
    if (state_machine.GetButtonPressed() != Response::Continue) {
        return LoopResult::RunCurrent;
    }
    return LoopResult::RunNext;
}

LoopResult CSelftestPart_ToolOffsets::state_home_park() {
    IPartHandler::SetFsmPhase(PhasesSelftest::ToolOffsets_pin_install_prepare);

    // Ensure tool will not hit calibration pin once installed
    marlin_server::enqueue_gcode("G1 G91");
    marlin_server::enqueue_gcode("G1 Z30");
    marlin_server::enqueue_gcode("G1 G90");

    // Ensure tool 0 is picked (no risky toolchange is needed with calibration pin installed)
    marlin_server::enqueue_gcode("T0 S1 L0 D0");
    marlin_server::enqueue_gcode("G28 O");

    // Park the nozzle for easier sheet removal
    marlin_server::enqueue_gcode_printf("T%d L0 D0", PrusaToolChanger::MARLIN_NO_TOOL_PICKED);
    return LoopResult::RunNext;
}

LoopResult CSelftestPart_ToolOffsets::state_wait_moves_done() {
    if (queue.has_commands_queued() || planner.processing()) {
        return LoopResult::RunCurrent;
    }
    return LoopResult::RunNext;
}

LoopResult CSelftestPart_ToolOffsets::state_ask_user_install_pin() {
    IPartHandler::SetFsmPhase(PhasesSelftest::ToolOffsets_wait_user_install_pin);
    return LoopResult::RunNext;
}

LoopResult CSelftestPart_ToolOffsets::state_wait_stable_temp() {
    IPartHandler::SetFsmPhase(PhasesSelftest::ToolOffsets_wait_stable_temp);
    if (all_nozzles_at_target()) {
        FanCoolingManager::reset();
        return LoopResult::RunNext;
    }
    FanCoolingManager::manage();
    return LoopResult::RunCurrent;
}

LoopResult CSelftestPart_ToolOffsets::state_calibrate() {
    IPartHandler::SetFsmPhase(PhasesSelftest::ToolOffsets_wait_calibrate);
    return LoopResult::RunNext;
}

/**
 * This state exists just because full_calibration() is a blocking call and we need to update FSM state
 * to let the user know that calibration is in progress.
 * The issue is that the fsm takes update after returning from a state function, so we cannot do it in one state.
 */
LoopResult CSelftestPart_ToolOffsets::state_finish_calibration() {
    bool calibration_success = full_calibration();
    if (!calibration_success) {
        return LoopResult::Fail;
    }
    return LoopResult::RunNext;
}

LoopResult CSelftestPart_ToolOffsets::state_final_park() {
    IPartHandler::SetFsmPhase(PhasesSelftest::ToolOffsets_wait_move_away);
    // Let user uninstall the pin
    marlin_server::enqueue_gcode("P0 S1"); // Park tool
    marlin_server::enqueue_gcode("G27"); // Park head
    marlin_server::enqueue_gcode("M18"); // Disable steppers
    return LoopResult::RunNext;
}

LoopResult CSelftestPart_ToolOffsets::state_ask_user_remove_pin() {
    IPartHandler::SetFsmPhase(PhasesSelftest::ToolOffsets_wait_user_remove_pin);
    return LoopResult::RunNext;
}
