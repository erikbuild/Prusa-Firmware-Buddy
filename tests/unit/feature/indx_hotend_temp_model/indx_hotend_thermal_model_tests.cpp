#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <feature/indx_hotend_temp_model/indx_hotend_thermal_model.hpp>

#include <cstdint>
#include <cmath>

using namespace indx;
using namespace Catch::Matchers;

using StepParams = HotendThermalModel::StepParams;

struct MetaParams {
    float heater_power_W = 0.f;

    bool update_nozzle_temp = true;

    void step(HotendThermalModel::StepParams &target) const {
        target.hotend_energy_consumed_uJ += uint32_t(heater_power_W * target.dt_s * 1000000.0f);
    }
};

/// Steps the model until modelled_nozzle_temp_C converges.
/// @returns the converged modelled_nozzle_temp_C
float converge(HotendThermalModel &model, StepParams &params, const MetaParams &meta_params) {
    INFO("converge");

    float prev_modelled_temp = NAN;

    for (size_t step = 0;; step++) {
        CAPTURE(step);

        const bool model_updated = model.step(params);
        meta_params.step(params);
        const float modelled_temp = model.modelled_nozzle_temp_C();

        if (meta_params.update_nozzle_temp) {
            params.nozzle_temp_C = modelled_temp;
        }

        REQUIRE(model_updated);

        const float temp_diff = std::abs(modelled_temp - prev_modelled_temp);
        CAPTURE(temp_diff);

        // Give the model a bit of a time to start reacting
        if (step > 1) {
            if (temp_diff < 0.1f) {
                // Converged - return the number
                return model.modelled_nozzle_temp_C();
            }

            // Does NOT apply - the model is not monotonous thanks to ambient_thermal_conductivity_W_C
            // Given stable parameters, the model should converge to a single number
            // REQUIRE(temp_diff < prev_modelled_temp_diff);
        }

        // The model should converge in a finite amount of time
        REQUIRE(step < 1000);

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
    CHECK_THAT(base_temp, WithinAbs(base_params.chamber_temp_C, 5));

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

TEST_CASE("indx_hotend_thermal_model::stuck_TPIS") {
    // Simulate a stuck TPIS - do not update nozzle_temp_C while heating
    // The model should still reach high temperatures that would trigger the protections in a reasonable time

    HotendThermalModel model;
    auto params = base_params;
    MetaParams meta_params {
        .heater_power_W = 20,
    };

    for (float time = 0; time < 20; time += params.dt_s) {
        model.step(params);
        meta_params.step(params);
    }
    CHECK(model.modelled_nozzle_temp_C() > 200);
}

TEST_CASE("indx_hotend_thermal_model::convergence") {
    // Check that the model converges on a variety of inputs

    HotendThermalModel model;
    auto params = base_params;
    params.board_temp_C = GENERATE(25.f, 50.f, 80.f);
    params.extruder_feedrate_mm_s = GENERATE(0.f, 10.f, 20.f, 30.f);
    params.print_fan_pwm = GENERATE(0, 30, 128, 255);
    params.chamber_temp_C = GENERATE(25.f, 47.f);

    MetaParams meta_params {
        .heater_power_W = GENERATE(0.f, 10.f, 20.f, 40.f),
    };
    converge(model, params, meta_params);
}

TEST_CASE("indx_hotend_thermal_model::nozzle_temp_effect") {
    // nozzle_temp_C is what the thermal model is testing against for thermal runaway
    // it shouldn't affect the model too much, just for gardual fine tuning

    constexpr MetaParams base_meta_params {
        .heater_power_W = 20,
    };
    constexpr auto test_params = [] {
        auto params = base_params;
        params.board_temp_C = 60;
        return params;
    }();
    static_assert(base_meta_params.update_nozzle_temp == true);

    const float base_temp = [&] {
        INFO("base_temp");

        HotendThermalModel model;
        auto params = test_params;
        return converge(model, params, base_meta_params);
    }();

    const float stuck_temp = [&] {
        INFO("stuck_temp");

        HotendThermalModel model;
        auto params = test_params;
        MetaParams meta_params = base_meta_params;
        meta_params.update_nozzle_temp = false;
        return converge(model, params, meta_params);
    }();

    const float diff = base_temp - stuck_temp;
    CAPTURE(base_temp, stuck_temp, diff);

    // The values should differ
    CHECK_THAT(diff, !WithinAbs(0, 2));

    // ... but not too much
    CHECK_THAT(diff, WithinAbs(0, 40));
}
