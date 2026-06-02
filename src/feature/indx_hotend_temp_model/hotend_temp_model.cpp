/// @file
#include "hotend_temp_model.hpp"

#include <freertos/timing.hpp>
#include <raii/scope_guard.hpp>

#include <feature/filament_tracker/filament_tracker.hpp>
#include <config_store/store_instance.hpp>
#include <feature/chamber/chamber.hpp>
#include <puppies/INDX.hpp>
#include <fanctl/fanctl.hpp>
#include <marlin_server.hpp>
#include <module/stepper.h>

namespace buddy {

INDXHotendTempModel &hotend_temp_model() {
    assert(osThreadGetId() == marlin_server::server_task);
    static INDXHotendTempModel instance;
    return instance;
}

void INDXHotendTempModel::step() {
    auto &indx_head = buddy::puppies::indx;
    const auto now_ms = freertos::millis();

    // !!! Needs to be stored before check() is called
    const auto last_step_ms = step_limiter_ms_.last_event();

    if (!step_limiter_ms_.check(now_ms)) {
        // Limit the step interval
        return;
    }

    float final_compensation_c = 0;

    // Set compensation in every case.
    // If we've failed to compute it for some reason, set it to zero for safety reasons.
    ScopeGuard compensation_sg = [&] {
        indx_head.set_hotend_temp_compensation(final_compensation_c);
    };

    const auto virtual_tool = VirtualToolIndex::currently_selected_opt();
    if (!virtual_tool.has_value()) {
        // Reset state once we pick a tool
        is_initialized_ = false;
        return;
    }

    // For multi-stepper tracking we'd need a different approach
    const AxisEnum e_axis = E0_AXIS;
    static_assert(E_STEPPERS == 1);

    const auto hotend_temp_c = indx_head.get_hotend_temp_uncompensated();
    const auto e_steps = stepper.position_from_startup(e_axis);
    const auto current_filament = FilamentType::for_tool(*virtual_tool);

    ScopeGuard last_sg = [&] {
        last_e_steps_ = e_steps;
        last_filament_ = current_filament;
    };

    if (!is_initialized_) {
        const auto heuristic_filament = FilamentType::for_tool_heuristic(*virtual_tool);

        if (heuristic_filament == FilamentType::none) {
            // No filament loaded, we don't want to compensate at all
            return;
        }

        compensator_.reset_state();

        // Do NOT use current_filament, use a smarter heuristic here
        compensator_.set_filament_parameters(indx::FilamentParameters::for_filament(heuristic_filament.parameters()));

        retracted_distance_mm_ = 0;

        is_initialized_ = true;

        // Keep the previous temperature compensation,
        // better to keep the previous value for one step than to potentially sharp jump to zero and then back to some value
        compensation_sg.disarm();

        // Wait one more step so that we can get a step proper step delta
        // last_sg ensures the last_XX members are properly initialized next round
        return;
    }

    if (current_filament != last_filament_) {
        // This is not perfect, something can still change filament parameters without changing the actual filament.
        // But we don't want to compose FilamentParameters together each step, it's too expensive.
        // If filament parameters change, we rely on the set_target_temperature being called, which then also calls reset_state.

        // Work normally this step, reinitialize on the next one
        reset_state();
    }

    // Note: Division by zero is guaranteed not to happen thanks to the step limiter
    const float step_delta_s = float(now_ms - last_step_ms) * 0.001f;

    // Track extruder turning and retraction
    float extruder_feedrate_mm_s;
    {
        const float e_delta_mm = (e_steps - last_e_steps_) / planner.settings.axis_steps_per_mm[e_axis];

        // Disregard retractions for feedrate computations. Only compute what actually went out of the nozzle
        // This logic somewhat duplicates what FilamentTracker is doing, but the problem is that FilamentTracker is before the Planner.
        // We need to work on live stepper data here.
        // !!! Needs to be computed before we update retracted_distance_mm_
        extruder_feedrate_mm_s = std::max(e_delta_mm - retracted_distance_mm_, 0.f) / step_delta_s;

        // Limit the feedrate to physical printer limits. We should never exceed this, but there might be some weird flukes if the time jumps weirdly
        extruder_feedrate_mm_s = std::min<float>(extruder_feedrate_mm_s, planner.settings.max_feedrate_mm_s[e_axis]);

        // Clamp the retraction tracking to extruder_to_nozzle_distance.
        // If we retract more, we're out of gears.
        retracted_distance_mm_ = std::clamp<float>(retracted_distance_mm_ - e_delta_mm, 0, buddy::FilamentTracker::extruder_to_nozzle_distance);
    }

    const ::indx_hotend_temp_compensation::StepParams step_params {
        .dt_s = step_delta_s,
        .chamber_temperature_c = chamber().current_temperature().value_or(25), // Originally 21, corrected for global warming
        .extruder_feedrate_mm_s = extruder_feedrate_mm_s,
        .hotend_temp_readout_c = hotend_temp_c,
        .hotend_temp_readout_dt_c_s = indx_head.get_hotend_temp_raw_c_dt_s(),
        .print_fan_pwm = static_cast<uint8_t>(indx_head.get_fan_pwm(0)),
    };

    // Will be applied by compensation_sg ScopeGuard
    final_compensation_c = compensator_.step(step_params);
}

void INDXHotendTempModel::reset_state() {
    is_initialized_ = false;
}

} // namespace buddy
