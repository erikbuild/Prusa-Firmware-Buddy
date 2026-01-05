#include "screen_filaments_loaded.hpp"
#include "screen_filament_detail.hpp"

#include <utils/string_builder.hpp>
#include <print_utils.hpp>
#include <ScreenHandler.hpp>
#include <img_resources.hpp>

#include <option/has_anfc.h>
#if HAS_ANFC()
    #include <feature/openprinttag/tool_tag.hpp>
#endif

MI_LOADED_FILAMENT::MI_LOADED_FILAMENT(DisplayFormat display_format, uint8_t tool)
    : IWindowMenuItem({}, nullptr, is_enabled_t::yes, is_hidden_t::no, expands_t::yes)
    , tool_(VirtualToolIndex::from_raw(tool))
    , display_format_(display_format) {

    should_open_submenu_ = (display_format == DisplayFormat::auto_submenu) && (get_num_of_enabled_tools() > 1);

    if (should_open_submenu_) {
        SetLabel(_("Loaded filaments"));

    } else {
        filament_type_ = config_store().get_filament_type(tool);

        StringBuilder sb(label_buffer_);
        if (display_format == DisplayFormat::auto_submenu) {
#if HAS_MINI_DISPLAY()
            // Longer text doesn't fit well on the mini display
            sb.append_string_view(_("Loaded"));
#else
            sb.append_string_view(_("Loaded filament"));
#endif
        } else {
            sb.append_string_view(_("Filament"));
            sb.append_printf(" %d", tool_.display_index());
        }

        sb.append_string(": ");
        filament_type_.build_name_with_info(sb);

        SetLabel(string_view_utf8::MakeRAM(label_buffer_.data()));
        set_enabled(filament_type_ != FilamentType::none);
        set_is_hidden(!tool_.is_enabled());
    }
}

void MI_LOADED_FILAMENT::click(IWindowMenu &) {
    if (should_open_submenu_) {
        Screens::Access()->Open<ScreenLoadedFilaments>();
    } else {
        Screens::Access()->Open(ScreenFactory::ScreenWithArg<ScreenFilamentDetail>(EncodedFilamentType(filament_type_)));
    }
}

void MI_LOADED_FILAMENT::Loop() {
#if HAS_ANFC()
    if (!should_open_submenu_) {
        using buddy::openprinttag::ToolTag;
        const auto ephemeral_tag = ToolTag::for_tool_ephemeral(tool_);
        const auto assigned_tag = ToolTag::for_tool_assigned(tool_);

        if (filament_type_ != FilamentType::none && ephemeral_tag != assigned_tag) {
            // Assigned OpenPrintTag is different to what is currently present
            SetIconId(&img::openprinttag_orange_16x16);

        } else if (ephemeral_tag.has_value()) {
            // Tool has a tag assigned and it is present
            SetIconId(&img::openprinttag_white_16x16);

        } else {
            // No tag present && no tag assigned
            SetIconId(nullptr);
        }
    }
#endif
}

ScreenLoadedFilaments::ScreenLoadedFilaments()
    : ScreenMenu(_("LOADED FILAMENTS")) {}
