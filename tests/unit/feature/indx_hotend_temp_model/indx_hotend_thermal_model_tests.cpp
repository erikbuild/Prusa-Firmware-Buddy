#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <feature/indx_hotend_temp_model/indx_hotend_thermal_model.hpp>

#include <cstdint>
#include <cmath>

using namespace indx;

using StepParams = HotendThermalModel::StepParams;

struct MetaParams {
    float heater_power_W = 0.f;

    void step(HotendThermalModel::StepParams &target) const {
        target.hotend_energy_consumed_uJ += uint32_t(heater_power_W * target.dt_s * 1000000.0f);
    }
};

/// Steps the model until modelled_nozzle_temp_C converges.
/// @returns the converged modelled_nozzle_temp_C
float converge(HotendThermalModel &model, StepParams &params, const MetaParams &meta_params) {
    INFO("converge");

    float prev_modelled_temp = NAN;
    float prev_modelled_temp_diff = NAN;

    for (size_t step = 0;; step++) {
        CAPTURE(step);

        const bool model_updated = model.step(params);
        meta_params.step(params);
        const float modelled_temp = model.modelled_nozzle_temp_C();
        params.nozzle_temp_C = modelled_temp;

        REQUIRE(model_updated);

        const float temp_diff = std::abs(modelled_temp - prev_modelled_temp);
        CAPTURE(temp_diff);

        // Give the model a bit of a time to start reacting
        if (step > 1) {
            if (temp_diff < 0.1f) {
                // Converged - return the number
                return model.modelled_nozzle_temp_C();
            }

            // Given stable parameters, the model should converge to a single number
            REQUIRE(temp_diff < prev_modelled_temp_diff);
        }

        // The model should converge in a finite amount of time
        REQUIRE(step < 1000);

        prev_modelled_temp_diff = temp_diff;
        prev_modelled_temp = modelled_temp;
    }
}

constexpr FilamentParameters filament {
    .linear_heat_capacity_J_C_m = 6.1f,
};

constexpr StepParams base_params {
    // So that the update would trigger each step
    .dt_s = HotendThermalModel::accum_interval_s,
    .hotend_energy_consumed_uJ = 0,
    .nozzle_temp_C = 25,
    .board_temp_C = 25,
    .extruder_feedrate_mm_s = 0,
    .print_fan_pwm = 0,
    .chamber_temp_C = 25,
    .filament = filament,
};

TEST_CASE("indx_hotend_thermal_model") {
    // Test that the model converges with default settings and the nozzle off
    const float base_temp = [&] {
        INFO("base_temp");

        HotendThermalModel model;
        auto params = base_params;
        MetaParams meta_params {};
        return converge(model, params, meta_params);
    }();
    CHECK_THAT(base_temp, Catch::Matchers::WithinAbs(base_params.chamber_temp_C, 5));

    const float slow_heat_temp = [&] {
        INFO("slow_heat_temp");

        HotendThermalModel model;
        auto params = base_params;
        MetaParams meta_params {
            .heater_power_W = 10,
        };
        return converge(model, params, meta_params);
    }();

    // With the heater on, we should converge on a way higher temperature
    CHECK(slow_heat_temp > base_temp + 20);

    const float fast_heat_temp = [&] {
        INFO("fast_heat_temp");

        HotendThermalModel model;
        auto params = base_params;
        MetaParams meta_params {
            .heater_power_W = 20,
        };
        return converge(model, params, meta_params);
    }();

    // More heating, more temperature
    CHECK(fast_heat_temp > slow_heat_temp + 20);

    const float burn_temp = [&] {
        INFO("burn_temp");

        HotendThermalModel model;
        auto params = base_params;
        MetaParams meta_params {
            .heater_power_W = 60,
        };
        return converge(model, params, meta_params);
    }();

    // With the heater on 100% power uncontrolled, we should get really, really, really hot
    CHECK(burn_temp > 900);

    const float burn_temp_fan = [&] {
        INFO("burn_temp_fan");

        HotendThermalModel model;
        auto params = base_params;
        params.print_fan_pwm = 255;
        MetaParams meta_params {
            .heater_power_W = 60,
        };
        return converge(model, params, meta_params);
    }();

    // ...and fan is not gonna save it too much
    CHECK(burn_temp_fan > 800);
    CHECK(burn_temp_fan < burn_temp);

    const float slow_heat_temp_with_fan = [&] {
        INFO("slow_heat_temp_with_fan");

        HotendThermalModel model;
        auto params = base_params;
        params.print_fan_pwm = 128;
        MetaParams meta_params {
            .heater_power_W = 10,
        };
        return converge(model, params, meta_params);
    }();

    // With fan on, we should converge on a lower temperature
    CHECK(slow_heat_temp_with_fan < slow_heat_temp);
    CHECK(slow_heat_temp_with_fan > base_temp);

    const float slow_heat_temp_with_filament = [&] {
        INFO("slow_heat_temp_with_filament");

        HotendThermalModel model;
        auto params = base_params;
        params.extruder_feedrate_mm_s = 5;
        MetaParams meta_params {
            .heater_power_W = 10,
        };
        return converge(model, params, meta_params);
    }();

    // With filament extruding, we should also converge on a lower temperature
    CHECK(slow_heat_temp_with_filament < slow_heat_temp);
    CHECK(slow_heat_temp_with_filament > base_temp);
}
