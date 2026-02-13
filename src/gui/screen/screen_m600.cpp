#include "screen_m600.hpp"
#include <general_response.hpp>
#include <window_msgbox.hpp>
#include <ScreenHandler.hpp>
#include <WindowMenuItems.hpp>
#include <screen_menu.hpp>
#include <img_resources.hpp>
#include <timing.h>
#include <config_store/store_instance.hpp>
#include <utils/string_builder.hpp>

#include <option/has_toolchanger.h>
#if HAS_TOOLCHANGER()
    #include <Marlin/src/module/prusa/toolchanger.h>
    #include <gui/dialogs/dialog_tool_select.hpp>
#endif

namespace {

bool enqueued = false; // Used to avoid multiple M600 enqueue

inline bool filament_change_dialog(uint8_t extruder) {
    StringViewUtf8Parameters<8> params;
    return MsgBoxQuestion(_("Change filament now?\n"
                            "Use same filament type as currently loaded.\n"
                            "Current filament type: %s")
                              .formatted(params, config_store().get_filament_type(extruder).parameters().name.data()),
               Responses_YesNo)
        == Response::Yes;
}

bool inject(const uint8_t tool) {
    if (!filament_change_dialog(tool)) {
        return false;
    }
    marlin_client::inject(GCodeLiteral("M600 P T%.0f", static_cast<float>(tool)));
    enqueued = true;
    return true;
};

} // namespace

MI_M600::MI_M600()
    : IWindowMenuItem(_(label), nullptr, is_enabled_t::yes, is_hidden_t::no,
#if HAS_TOOLCHANGER()
        (prusa_toolchanger.is_toolchanger_enabled()) ? expands_t::yes :
#endif
                                                     expands_t::no) {
    set_icon_position(IconPosition::right);
}

void MI_M600::click([[maybe_unused]] IWindowMenu &window_menu) {
#if HAS_TOOLCHANGER()
    if (prusa_toolchanger.is_toolchanger_enabled()) {
        const auto tool = select_tool_dialog({
            .allow_return = true,
        });
        if (tool.has_value() && inject(tool->to_raw())) {
            Screens::Access()->Close();
        }
        return;
    }
#endif
    match(
        marlin_vars().active_extruder.get(),
        [](VirtualToolIndex virtual_tool) { inject(virtual_tool.to_raw()); },
        [](NoTool) { assert(false); });
}

void MI_M600::Loop() {
    update_enqueued_icon();
    handle_enable_state();
}

void MI_M600::update_enqueued_icon() {
    if (enqueued) {
        SetIconId(img::spinner_16x16_animated());
    } else {
        SetIconId(nullptr);
    }
}

void MI_M600::handle_enable_state() {

    // This is a little bit of a hack - instead of checking if M600 was executed
    // we are checking if nothing is in the inject queue, which is much weaker assumption
    // We need more proper information dumping from marlin server about queue to be able to do it the correct way
    if (marlin_vars().inject_queue_empty) {
        enqueued = false;
    }
    // M600 during printing is enabled the moment after first layer started printing
    // M600 is incompatible with initializing gcodes such as G29 a G28
    set_enabled(!enqueued && (marlin_vars().max_printed_z > 0));
}
