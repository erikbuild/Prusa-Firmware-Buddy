#include "config_features.h"
#include <feature/filament_sensor/filament_sensors_handler.hpp>
#include <config_store/store_instance.hpp>
#include <option/has_mmu2.h>
#include "../../../lib/Marlin/Marlin/src/Marlin.h"
#include "../../../lib/Marlin/Marlin/src/module/motion.h"
#include "../../../lib/Marlin/Marlin/src/module/planner.h"
#include "../../../lib/Marlin/Marlin/src/module/temperature.h"
#include "pause_stubbed.hpp"
#include <feature/filament_sensor/filament_sensors_handler.hpp>
#include "M70X.hpp"
#include <option/has_toolchanger.h>

#if HAS_CHAMBER_API()
    #include <feature/chamber/chamber.hpp>
#endif

static FSMResponseVariant preheatTempUnKnown(PreheatData preheat_data) {
    marlin_server::FSM_Holder holder { PhasesPreheat::UserTempSelection, preheat_data.serialize() };

    while (true) {
        if (const auto ret = marlin_server::get_response_variant_from_phase(PhasesPreheat::UserTempSelection)) {
            return ret;
        }
        if (preheat_data.mode == PreheatMode::autoload && FSensors_instance().sensor_state(LogicalFilamentSensor::primary_runout) == FilamentSensorState::NoFilament) {
            return FSMResponseVariant::make(Response::Abort);
        }

        // If someone inserts a filament while in the actual "Preheat" menu, abort (so that we can spin up the load FSM)
        if (preheat_data.mode == PreheatMode::preheat && FSensors_instance().IsAutoloadInProgress()) {
            return FSMResponseVariant();
        }

        idle(true);
    }
}

static FSMResponseVariant evaluate_preheat_conditions(PreheatData preheat_data) {
    const auto filament_type = [&] {
        if ((preheat_data.mode != PreheatMode::unload) && (preheat_data.mode != PreheatMode::purge)) {
            // We cannot know the temperature, and thus must ask the user
            return FilamentType::none;
        }

        return match(
            preheat_data.tool, //
            [](VirtualToolIndex i) { return config_store().get_filament_type(i); }, //
            [](AllTools) { return FilamentType::none; });
    }();

    if (filament_type != FilamentType::none) {
        // We know the filament parameters, no need to ask the user
        return FSMResponseVariant::make(filament_type);

    } else {
        // we need to ask the user for temperature
        return preheatTempUnKnown(preheat_data);
    }
}

std::pair<std::optional<PreheatStatus::Result>, FilamentType> filament_gcodes::preheat(PreheatData preheat_data, PreheatBehavior preheat_arg) {
    const FSMResponseVariant response = evaluate_preheat_conditions(preheat_data);

    const auto physical_tool = to_physical_tool_index<AllTools>(preheat_data.tool);

    if (response.holds_alternative<FilamentType>()) {
        const FilamentType filament = response.value<FilamentType>();
        preheat_to(filament, physical_tool, preheat_arg);
        return { std::nullopt, filament };
    }

    switch (response.value_or<Response>(Response::_none)) {

    case Response::Abort:
        return { PreheatStatus::Result::Aborted, FilamentType::none };

    case Response::Cooldown:
        return { PreheatStatus::Result::CooledDown, FilamentType::none };

    default:
        // should not happen
        return { PreheatStatus::Result::Error, FilamentType::none };
    }
}

filament_gcodes::PreheatBehavior filament_gcodes::PreheatBehavior::for_filament_change(bool force_temp) {
    const bool preheat_all = config_store().filament_change_preheat_all.get();

    return PreheatBehavior {
        .force_temp = force_temp,
        .preheat_bed = preheat_all,
#if HAS_CHAMBER_API()
        .set_chamber_temperature = preheat_all,
#endif
    };
}

void filament_gcodes::preheat_to(FilamentType filament, std::variant<PhysicalToolIndex, AllTools> tools, PreheatBehavior preheat_arg) {
    const FilamentTypeParameters fil_cnf = filament.parameters();

    bool hotend_temp_changed = false;

    // change temp only if it is lower than currently loaded filament
    // TODO: Why? This is now very problematic with the new heatbreak and chamber params
    for (const PhysicalToolIndex tool : tool_index_iterator(tools)) {
        if (preheat_arg.force_temp || thermalManager.degTargetHotend(tool) < fil_cnf.nozzle_temperature) {
            hotend_temp_changed = true;
            thermalManager.setTargetHotend(fil_cnf.nozzle_temperature, tool);
        }

#if HAS_FILAMENT_HEATBREAK_PARAM()
        thermalManager.setTargetHeatbreak(fil_cnf.heatbreak_temperature, tool);
#endif
    }

    if (hotend_temp_changed && preheat_arg.preheat_bed && (preheat_arg.force_temp || (thermalManager.degTargetBed() < fil_cnf.heatbed_temperature))) {
        thermalManager.setTargetBed(fil_cnf.heatbed_temperature);
    }

#if HAS_CHAMBER_API()
    if (preheat_arg.set_chamber_temperature) {
        buddy::chamber().set_target_temperature(fil_cnf.chamber_target_temperature);
    }
#endif
}

void filament_gcodes::M1700_no_parser(const M1700Args &args) {
    InProgress progress;
    const FSMResponseVariant response_variant = preheatTempUnKnown(PreheatData::make(args.mode, args.tool, args.preheat));

    // autoload ocurred
    if (!response_variant) {
        return;
    }

    const Response response = response_variant.value_or<Response>(Response::_none);
    if (response == Response::Abort) {
        PreheatStatus::SetResult(PreheatStatus::Result::Aborted);
        return;
    }

    const FilamentType filament = response_variant.value_or<FilamentType>(FilamentType::none);
    const FilamentTypeParameters fil_cnf = filament.parameters();

    auto iterator = tool_index_iterator(args.tool);
    if (response == Response::Cooldown) {
        // Cooldown applies for all tools
        iterator = VirtualToolIndex::Iterator::make_all();
    }
    iterator = iterator.skip_all_disabled();

    for (VirtualToolIndex virtual_tool : iterator) {
        const PhysicalToolIndex physical_tool = virtual_tool.to_physical();
        thermalManager.setTargetHotend(args.enforce_target_temp ? fil_cnf.nozzle_temperature : fil_cnf.nozzle_preheat_temperature, physical_tool);
#if HAS_FILAMENT_HEATBREAK_PARAM()
        if (args.set_heatbreak) {
            thermalManager.setTargetHeatbreak(fil_cnf.heatbreak_temperature, physical_tool);
        }
#endif
    }

    if (args.preheat_bed) {
        thermalManager.setTargetBed(fil_cnf.heatbed_temperature);
    }

#if HAS_CHAMBER_API()
    if (args.preheat_chamber) {
        buddy::chamber().set_target_temperature(fil_cnf.chamber_target_temperature);
    }
#endif
    // cooldown pressed
    if (filament == FilamentType::none) {
        thermalManager.set_fan_speed(0, 0);

    } else if (!axes_home_level.is_homed(Z_AXIS, AxisHomeLevel::imprecise)) {
        unhomed_z_lift(10);
    }

    if (args.save && response != Response::Cooldown) {
        for (const VirtualToolIndex tool : iterator) {
            config_store().set_filament_type(tool, filament);
        }
    }

    // store result, so other threads can see it
    PreheatStatus::SetResult(PreheatStatus::Result::DoneNoFilament);

    // we might want to set filament type even with preheat, if so do:
    // Filaments::SetToBeLoaded(filament);
}
