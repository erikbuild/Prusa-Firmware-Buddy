/// @file
#include "local_hotend.hpp"

#include <module/thermistor/thermistors.h>
#include <module/temperature.h>
#include <module/temperature/temperature_declares.hpp>

#include <option/has_planner.h>
#if HAS_PLANNER()
    #include <module/planner.h>
    #include <module/stepper.h>
#endif

LocalHotend::LocalHotend(PhysicalToolIndex tool, const Config *config)
    : BaseHotend(tool, &config->base_config)
    , local_config_(*config)
    , nozzle_raw_temp_range_ { MarlinTemptableRawMinMax::compute(local_config_.nozzle_temp_table, base_config_.min_nozzle_temp, base_config_.max_nozzle_temp) } //
{
}

void LocalHotend::manage() {
    nozzle_temp_ = marlin_temptable_lookup(local_config_.nozzle_temp_table, nozzle_raw_temp_);

    // !!! MUST be called after temps are set properly
    BaseHotend::manage();

    // Manage temperature regulation
    {
        auto &t = thermalManager;

#if ENABLED(PID_EXTRUSION_SCALING)
        static_assert(HAS_PLANNER());

        const bool is_current_tool = stdext::holds_value(PhysicalToolIndex::currently_selected(), tool_);

        // Note: This seems like a BS. manage_heater is called in idle(), not in the timer ISR
        // We should instead be using actual measured intervals between manage() calls
        // Or just remove this completely, as it doesn't seem to be working and noone noticed
        constexpr float sample_frequency = TEMP_TIMER_FREQUENCY / MIN_ADC_ISR_LOOPS / OVERSAMPLENR;
        constexpr float distance_to_volume = std::numbers::pi_v<float> * std::pow(DEFAULT_NOMINAL_FILAMENT_DIA / 2, 2.f);
        constexpr float distance_to_volume_per_second = distance_to_volume * sample_frequency;

        uint32_t e_position = stepper.position(E_AXIS);
        const float e_volume_delta = is_current_tool ? (e_position - last_e_position_) * planner.mm_per_step[E_AXIS] * distance_to_volume_per_second : 0;
        last_e_position_ = e_position;
#endif

        HotendRegulatorResult regulation_result {
            .pid_output = 0,
            .feed_forward = 0,
        };

        if (nozzle_temp() > base_config_.min_nozzle_temp && nozzle_temp() < base_config_.max_nozzle_temp) {
            static_assert(PhysicalToolIndex::count == 1);
            regulation_result = nozzle_regulator_.get_pid_output_hotend(HotendRegulatorArgs {
                .pid = t.temp_hotend[tool_].pid,
                .hotend_index = tool_.to_raw(),
                .fan_speed = t.fan_speed[0], // FIXME: Bit of a cockup if we have multiple hotends.
                    .current_temp = nozzle_temp(),
                .target_temp = nozzle_target_temp(),
#if ENABLED(PID_EXTRUSION_SCALING)
                .e_volume_delta = (thermalManager.extrusion_scaling_enabled && is_current_tool) ? e_volume_delta : 0,
#endif
            });
        }

        t.temp_hotend[tool_].soft_pwm_amount = static_cast<int>(regulation_result.pid_output) >> soft_pwm_bit_shift;
#if ENABLED(MODEL_DETECT_STUCK_THERMISTOR)
        thermal_model_protection_.step(regulation_result.pid_output, regulation_result.feed_forward);
        thermal_model_protection_ok_ = thermal_model_protection_.is_ok();
#endif
    }
}

void LocalHotend::isr_on_readings_ready() {
    auto &th = thermalManager.temp_hotend[tool_.to_raw()];

    // Note: Before Hotend refactoring, updating the raw value was waiting for temp_meas_ready
    // Now, we are using std::atomic like sane people, so it shouldn't be necessary
    nozzle_raw_temp_ = th.acc;
    th.acc = 0;

    const bool heater_on = (nozzle_target_temp() > 0
#if ENABLED(PIDTEMP)
        || th.soft_pwm_amount > 0
#endif
    );

    nozzle_raw_temp_range_.check_temperror(nozzle_raw_temp_, (heater_ind_t)tool_.to_raw(), heater_on);
}
