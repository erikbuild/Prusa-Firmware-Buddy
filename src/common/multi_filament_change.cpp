#include "multi_filament_change.hpp"

#include <gcode_info.hpp>
#include <module/prusa/spool_join.hpp>
#include <module/prusa/tool_mapper.hpp>
#include <base64/base64.hpp>
#include <string_builder.hpp>
#include <mbedtls/base64.h>

#include <gcode/gcode.h>
#include <marlin_server.hpp>

namespace multi_filament_change {

namespace {
    constexpr size_t base64_data_length = base64::encoded_length(sizeof(Config));
}

Config config_from_current_print_setup() {
    Config result;

    const auto &gcode_info = GCodeInfo::getInstance();

    for (const auto virtual_tool : VirtualToolIndex::all()) {
        const auto main_virtual_tool = spool_join.get_first_spool_1_from_chain(virtual_tool);
        const auto gcode_tool = stdext::get_optional<GcodeToolIndex>(tool_mapper.to_gcode(main_virtual_tool));

        // Not assigned -> keep as 'don't change'
        if (!gcode_tool.has_value()) {
            continue;
        }

        const auto &tool_info = gcode_info.get_extruder_info(*gcode_tool);

        auto &item = result[virtual_tool];

        assert(tool_info.used()); // otherwise bug in mapping
        item.color = tool_info.extruder_colour;

        const auto &opt_name = tool_info.filament_name;
        if (opt_name.empty()) {
            continue;
        }

        // Only preselect if we don't have it already
        if (config_store().get_filament_type(virtual_tool).matches(opt_name)) {
            continue;
        }

        item.action = multi_filament_change::Action::change;

        // We're loading a new filament, do not fallback into ad-hoc one -> extruder_index = std::nullopt
        item.new_filament = FilamentType::from_name(opt_name);
    }

    return result;
}

std::optional<Config> config_from_gcode(GCodeBasicParser &parser) {
    // Bit of a hack - we are encoding the Config as Base64
    // Base64 has special characters '+', '/' and '='
    // These don't interfere with the G-Code special characters, which is handy for us

    Config config;
    const std::span<std::byte> config_span { reinterpret_cast<std::byte *>(&config), sizeof(config) };
    static_assert(std::is_trivially_copyable_v<Config>);

    size_t pos = 0;

    // The Base64Decoder is used in plain gcode thumbnail parser,
    // so reusing it here takes less flash than using the mbed_tls_decode (which is not used anywhere)
    base64::Base64Decoder decoder;
    for (const char ch : parser.body()) {
        std::byte byte;
        using Result = base64::Base64Decoder::DecodeResult;
        switch (decoder.decode(ch, byte)) {

        case Result::error:
            return std::nullopt;

        case Result::new_output:
            if (pos >= config_span.size()) {
                return std::nullopt;
            }
            config_span[pos++] = byte;
            break;

        case Result::no_output:
            break;
        }
    }

    if (!decoder.finalize() || pos != config_span.size()) {
        return std::nullopt;
    }

    return config;
}

void config_to_gcode(const Config &config, StringBuilder &sb) {
    static_assert(gcode_command == GCodeCommand { .letter = 'M', .codenum = 9934 });
    // The space is necessary there in case the Base64 code would start with a digit
    constexpr const char *prefix = "M9934 ";

    // Bit of a hack - we are encoding the Config as Base64
    // Base64 has special characters '+', '/' and '='
    // These don't interfere with the G-Code special characters, which is handy for us

    constexpr size_t expected_gcode_length = strlen(prefix) + base64_data_length;

    // If you get this assert fail, you gotta optimize the Config struct to fit into MAX_CMD_SIZE
    // Suggestion to fix:
    // - Turn ConfigItem::FilamentType into EncodedFilamentType, it will take 1 B instead of 2 B
    // - Put ConfigItem::colors into a separate array. Color takes 4 B, causing 4 B align on the whole struct.
    //   (other fields in the struct take 2 B, so we're wasting 2 B per ConfigItem)
    static_assert(expected_gcode_length + 1 <= MAX_CMD_SIZE, "The MultiFilamentChange gcode would not fit into the queues");

    sb.append_string(prefix);

    const auto encode_buf = sb.alloc_chars(base64_data_length);
    if (!encode_buf) {
        bsod_unreachable();
    }

    // mbedtls_base64_encode is used in websocket.cpp,
    // so reusing it here takes less flash than using the Base64Encoder (which is not used anywhere)
    size_t olen = 0;

    // mbedtls_base64_encode has this slightly annoying property that it also appends \0 at the end
    // We're technically going OOB here when passing base64_data_length + 1 (to account for the \0), but StringBuilder always keeps a space for a terminating \0, so this should be completely fine
    mbedtls_base64_encode(reinterpret_cast<uint8_t *>(encode_buf), base64_data_length + 1, &olen, reinterpret_cast<const uint8_t *>(&config), sizeof(Config));
    static_assert(std::is_trivially_copyable_v<Config>);
    assert(olen == base64_data_length);
}

void execute(const Config &tool_config) {
    // If we have nothing to do, we can exit right now
    if (std::all_of(tool_config.begin(), tool_config.end(), [](const auto &i) { return i.action == Action::keep; })) {
        return;
    }

    marlin_server::FSM_Holder fsm { PhaseWait::print_status_message };

    StrongIndexArray<FilamentType, VirtualToolIndex::count, VirtualToolIndex, VirtualToolIndex::to_raw_static> old_filaments;
    for (auto tool : VirtualToolIndex::all()) {
        old_filaments[tool] = config_store().get_filament_type(tool);
    }

#if HAS_TOOLCHANGER()
    // Start preheating all nozzles
    for (auto tool : VirtualToolIndex::all()) {
        const auto &config = tool_config[tool];
        if (config.action == Action::keep) {
            continue;
        }

        const uint16_t temperature = max(config.new_filament.parameters().nozzle_temperature, old_filaments[tool].parameters().nozzle_temperature);
        thermalManager.setTargetHotend(temperature, tool.to_physical());
    }
#endif

    // Lift Z to prevent unparking and parking of each tool
    GcodeSuite::process_subcommands_now_P("G27 P0 Z40");

    /* MMU2 Reimplementation

        MMU should be in unloaded state right now (as every start of print)
        For all selected tools (in this case tool == MMU's filament slot) change filament
        This means MMU will go through selected tools and load to the slot (NOT load to extruder)
        M704 for filament pre-load does not support color specification
    */

    // Carry out the changes
    for (auto tool : VirtualToolIndex::all()) {
        const auto &config = tool_config[tool];
        switch (config.action) {

        case Action::keep:
            break;

        case Action::unload: {
#if HAS_MMU2()
            config_store().set_filament_type(tool, FilamentType::none);
#else
            ArrayStringBuilder<MAX_CMD_SIZE> command_builder;
            command_builder.append_printf("M702 T%d W2", tool.to_raw());
            GcodeSuite::process_subcommands_now_P(command_builder.str());
#endif
            break;
        }

        case Action::change: {
#if HAS_MMU2()
            ArrayStringBuilder<MAX_CMD_SIZE> command_builder;
            command_builder.append_printf("M704 P%d", tool.to_raw());
            GcodeSuite::process_subcommands_now_P(command_builder.str());

            config_store().set_filament_type(tool, config.new_filament);
#else
            ArrayStringBuilder<MAX_CMD_SIZE> command_builder;

            // M1600 - filament change (doesn't ask for unload)
            // M701 - filament load
            command_builder.append_printf((old_filaments[tool] != FilamentType::none) ? "M1600 S\"%s\" T%d R" : "M701 S\"%s\" T%d W2", config.new_filament.parameters().name.data(), tool.to_raw());

            if (config.color.has_value()) {
                command_builder.append_printf(" O%" PRIu32, config.color->raw);
            }

            assert(command_builder.is_ok());
            GcodeSuite::process_subcommands_now_P(command_builder.str());
#endif
            break;
        }
        }
    }
}
} // namespace multi_filament_change
