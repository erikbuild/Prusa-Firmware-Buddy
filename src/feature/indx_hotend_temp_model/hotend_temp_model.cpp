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

// We have this constant on two places because of modules isolation, make sure they have the same value
static_assert(indx::HotendThermalModel::hotend_induction_efficiency == buddy::puppies::Indx::hotend_induction_efficiency);

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

    // !!! MUST be read before the temperatures themselves to avoid race conditions
    const bool temps_valid = indx_head.get_temps_valid();

    const auto hotend_temp_uncompensated_c = indx_head.get_hotend_temp_uncompensated();
    const auto hotend_temp_compensated_c = indx_head.get_hotend_temp_compensated();
    const auto hotend_temp_raw_dt_c_s = indx_head.get_hotend_temp_raw_c_dt_s();
    const auto hotend_energy_consumed_uJ = indx_head.get_hotend_energy_consumed_uJ();
    const auto board_temperature_c = indx_head.get_board_temperature();
    const auto print_fan_pwm = static_cast<uint8_t>(indx_head.get_fan_pwm(0));

    const auto e_steps = stepper.position_from_startup(e_axis);
    const auto current_filament = FilamentType::for_tool(*virtual_tool);

    // !!! MUST be read after everything else from the puppy to avoid race conditions
    // This being the last thing is intended to catch indx head resets mid read
    const auto puppy_reset_count = indx_head.get_reset_counter();

    ScopeGuard last_sg = [&] {
        last_e_steps_ = e_steps;
        last_filament_ = current_filament;
        last_puppy_reset_count_ = puppy_reset_count;
    };

    if (!temps_valid || (puppy_reset_count != last_puppy_reset_count_)) {
        // Either the puppy has been reset (which invalidates readings)
        // or the readings have not yet become valid after indx heat boot
        // We can't continue, wait till everything is valid

        // Reset all models
        is_initialized_ = false;

        // Some of the readings are invalid - reinitialize the model when they become valid
        return;
    }

    if (current_filament != last_filament_) {
        // This is not perfect, something can still change filament parameters without changing the actual filament.
        // But we don't want to compute FilamentParameters together each step, it's too expensive.
        filament_data_update_pending_ = true;
    }

    if (!is_initialized_) {
        compensator_.reset_state();
        thermal_model_.reset_state();

        retracted_distance_mm_ = 0;

        is_initialized_ = true;
        filament_data_update_pending_ = true;

        // Keep the previous temperature compensation,
        // better to keep the previous value for one step than to potentially sharp jump to zero and then back to some value
        compensation_sg.disarm();

        // Wait one more step so that we can get a step proper step delta
        // last_sg ensures the last_XX members are properly initialized next round
        return;
    }

    if (filament_data_update_pending_) {
        const auto heuristic_filament = FilamentType::for_tool_heuristic(*virtual_tool);
        indx::FilamentParameters new_params;

        if (heuristic_filament == FilamentType::none) {
            new_params = indx::FilamentParameters::for_no_filament();
        } else {
            // Do NOT use current_filament, use a smarter heuristic here
            new_params = indx::FilamentParameters::for_filament(heuristic_filament.parameters());
        }

        if (new_params != filament_params_) {
            // Filament changed, reset the models
            filament_params_ = new_params;
            compensator_.reset_state();
            thermal_model_.reset_state();
        }

        filament_data_update_pending_ = false;
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
        const float extruded_filament_mm = std::max(e_delta_mm - retracted_distance_mm_, 0.f);

        // Limit the feedrate to physical printer limits. We should never exceed this, but there might be some weird flukes if the time jumps weirdly
        extruder_feedrate_mm_s = std::min<float>(extruded_filament_mm / step_delta_s, planner.settings.max_feedrate_mm_s[e_axis]);

        // Clamp the retraction tracking to extruder_to_nozzle_distance.
        // If we retract more, we're out of gears.
        retracted_distance_mm_ = std::clamp<float>(retracted_distance_mm_ - e_delta_mm, 0, buddy::FilamentTracker::extruder_to_nozzle_distance);
    }

    const auto chamber_temp_C = chamber().current_temperature().value_or(25); // Originally 21, corrected for global warming

    // Manage hotend temp compensation
    {
        const ::indx_hotend_temp_compensation::StepParams step_params {
            .dt_s = step_delta_s,
            .chamber_temperature_c = chamber_temp_C,
            .extruder_feedrate_mm_s = extruder_feedrate_mm_s,
            .hotend_temp_readout_c = hotend_temp_uncompensated_c,
            .hotend_temp_readout_dt_c_s = hotend_temp_raw_dt_c_s,
            .print_fan_pwm = print_fan_pwm,
            .filament = filament_params_,
        };

        // Will be applied by compensation_sg ScopeGuard
        final_compensation_c = compensator_.step(step_params);
    }

    // Manage hotend thermal model
    {
        // hotend_temp_uncompensated_c is at TPIS, get_hotend_temp_compensated is at the nozzle tip
        // Hotend thermal model needs an "average" temperature that is somewhere in the middle of the nozzle
        constexpr float hotend_temp_compensation_weight = 0.5f;
        const float model_ref_temp_c = hotend_temp_uncompensated_c * (1.f - hotend_temp_compensation_weight) + hotend_temp_compensated_c * hotend_temp_compensation_weight;

        const indx::HotendThermalModel::StepParams step_params {
            .dt_s = step_delta_s,
            .hotend_energy_consumed_uJ = hotend_energy_consumed_uJ,
            .nozzle_temp_C = model_ref_temp_c,
            .board_temp_C = float(board_temperature_c),
            .extruder_feedrate_mm_s = extruder_feedrate_mm_s,
            .print_fan_pwm = print_fan_pwm,
            .chamber_temp_C = chamber_temp_C,
            .filament = filament_params_,
        };

        const bool model_updated = thermal_model_.step(step_params);

        if (model_updated) {
            constexpr float max_thermal_model_discrepancy_C = 60;

            // Only check if the modelled temperature is higher than the measured one
            // Colder model temperature does not pose a safety problem
            if (thermal_model_.modelled_nozzle_temp_C() > hotend_temp_uncompensated_c + max_thermal_model_discrepancy_C) {
                fatal_error(ErrCode::ERR_TEMPERATURE_HOTEND_THERMAL_RUNAWAY);
            }
        }
    }
}

void INDXHotendTempModel::reset_state() {
    is_initialized_ = false;
}

void INDXHotendTempModel::update_filament_params() {
    filament_data_update_pending_ = true;
}
} // namespace buddy
