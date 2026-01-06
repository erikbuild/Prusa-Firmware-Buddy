#include "screen_opt_info.hpp"
#include "screen_opt_info_private.hpp"

#include <window_msgbox.hpp>
#include <ScreenHandler.hpp>
#include <feature/openprinttag/requests_read_multi.hpp>
#include <feature/openprinttag/data_utils.hpp>
#include <feature/openprinttag/filament_usage_tracker/filament_usage_tracker.hpp>
#include <string_builder.hpp>
#include <screen/openprinttag/opt_request_wizard.hpp>
#include <screen/openprinttag/screen_opt_filament_detail.hpp>
#include <img_resources.hpp>

namespace buddy::openprinttag {

constinit const std::array<const char *, 2> MenuItemFilamentTracking::values {
    N_("No"),
    N_("Yes"),
};

MenuItemFilamentTracking::MenuItemFilamentTracking(VirtualToolIndex tool)
    : IWindowMenuItem(_("Filament tracking"))
    , tool_(tool) {
    extension_width = std::max(_(values[0]).computeNumUtf8Chars(), _(values[1]).computeNumUtf8Chars()) * resource_font(font)->w + w_for_icon;
}

void MenuItemFilamentTracking::Loop() {
    const auto assigned_tag = ToolTag::for_tool_assigned(tool_);
    const auto ephemeral_tag = ToolTag::for_tool_ephemeral(tool_);
    const auto new_is_tracking = assigned_tag && (assigned_tag == ephemeral_tag) && buddy::openprinttag::is_filament_usage_tracking(tool_);

    if (is_tracking_ != new_is_tracking) {
        is_tracking_ = new_is_tracking;
        InValidateExtension();
    }
}

void MenuItemFilamentTracking::printExtension(Rect16 extension_rect, [[maybe_unused]] Color color_text, Color color_back, ropfn raster_op) const {
    if (is_tracking_) {
        render_text_align(extension_rect, _(values[1]), font, color_back, COLOR_GREEN, GuiDefaults::MenuPaddingSpecial, Align_t::RightCenter());

    } else {
        render_icon_align(extension_rect, &img::arrow_right_10x16, color_back, icon_flags(Align_t::RightCenter(), raster_op));
        render_text_align(extension_rect - Rect16::W_t(w_for_icon), _(values[0]), font, color_back, COLOR_RED, GuiDefaults::MenuPaddingSpecial, Align_t::RightCenter());
    }
}

void MenuItemFilamentTracking::click(IWindowMenu &) {
    const auto assigned_tag = ToolTag::for_tool_assigned(tool_);
    const auto ephemeral_tag = ToolTag::for_tool_ephemeral(tool_);

    const char *msg = nullptr;

    if (!ephemeral_tag) {
        msg = N_("No OpenPrintTag detected for the tool/slot.");

    } else if (ephemeral_tag != assigned_tag) {
        msg = N_("Currently detected OpenPrintTag is different to the assigned one present during filament load.");

    } else if (!buddy::openprinttag::is_filament_usage_tracking(tool_)) {
        msg = N_("The tag is corrupt, locked, missing lenght/weight data or otherwise unsuitable.");
    }

    assert((msg != nullptr) == is_tracking_);
    if (msg) {
        MsgBoxError(_(msg), Responses_Ok);
    }
}

WindowMenuOPTInfo::WindowMenuOPTInfo(window_t *parent, Rect16 rect)
    : WindowMenuVirtual(parent, rect, CloseScreenReturnBehavior::yes) {
    // setup_items is called from ScreenOTPInfo
}

int WindowMenuOPTInfo::item_count() const {
    return index_mapping_.total_item_count();
}

void WindowMenuOPTInfo::setup_item(ItemVariant &variant, int index) {
    const auto item = index_mapping_.from_index(index);
    switch (item.item) {

    case Item::return_:
        variant.emplace<MI_RETURN>();
        break;

    case Item::data_section:
        data_items_[item.pos_in_section](variant);
        break;

    case Item::filament_tracking: {
        variant.emplace<MenuItemFilamentTracking>(screen_->tool_);
        break;
    }

    case Item::print_parameters:
        const auto callback = [this] {
            Screens::Access()->Open(screen_openprinttag_filament_detail_creator(*screen_->tag_));
        };
        variant.emplace<WindowMenuCallbackItem>(_("Printing Parameters"), callback, nullptr, expands_t::yes);
        break;
    }
}

ScreenOPTInfo::ScreenOPTInfo(VirtualToolIndex tool)
    : ScreenMenuBase(nullptr, _("OPENPRINTTAG INFO"), EFooter::Off)
    , tool_(tool) {

    menu.menu.screen_ = this;
    menu.menu.setup_items();

    // First scan needs to be delayed (cannot be in the constructor)
    scan_pending_ = true;
}

void ScreenOPTInfo::screenEvent([[maybe_unused]] window_t *sender, GUI_event_t event, [[maybe_unused]] void *param) {
    switch (event) {

    case GUI_event_t::LOOP:
        if (scan_pending_) {
            scan_pending_ = false;
            scan();
        }
        break;

    default:
        break;
    }

    ScreenMenuBase::screenEvent(sender, event, param);
}

bool ScreenOPTInfo::scan() {
    tag_ = ToolTag::for_tool_ephemeral(tool_);
    if (!tag_) {
        StringViewUtf8Parameters<4> fmt;
        MsgBoxError(_("No OpenPrintTag detected for slot %i").formatted(fmt, tool_.to_raw() + 1), Responses_Ok);
        close_screen();
        return false;
    }

    const auto tag = *tag_;

    using MultiRequest = MultiReadFieldRequest<
        MainField::material_name,
        MainField::brand_name,
        AmountsInfo::Requirements {},
        AbbreviationInfo::Requirements {}>;

    MultiRequest req { tag };

    if (!multirequest_with_troubleshooting(req)) {
        close_screen();
        return false;
    }

    AmountsInfo weights { req };
    AbbreviationInfo type { req };

    auto &menu = this->menu.menu;

    menu.data_items_.clear();

    if (auto val = req.result<MainField::brand_name>()) {
        add_string_item(N_("Brand"), *val, brand_name_buffer_);
    }

    if (auto val = req.result<MainField::material_name>()) {
        add_string_item(N_("Material Name"), *val, material_name_buffer_);
    }

    if (!type.abbreviation.empty()) {
        const std::string_view abbr = type.abbreviation;
        const auto default_abbr = req.result<MainField::material_type>().transform([](auto item) { return ::openprinttag::enum_item_name(item); });

        StringBuilder sb(abbreviation_buffer_);
        sb.append_printf("%.*s", abbr.size(), abbr.data());
        if (default_abbr.has_value() && default_abbr != type.abbreviation) {
            sb.append_printf(" (%.*s)", default_abbr->size(), default_abbr->data());
        }

        add_string_item(N_("Abbreviation"), sb.str(), abbreviation_buffer_);
    }

    if (weights.full_weight_g.has_value() && weights.remaining_weight_g.has_value() && weights.full_weight_g != weights.remaining_weight_g) {
        add_fmt_item<N_("Remaining Weight"), "%.0f/%.0f g"_tstr, float, float>(*weights.remaining_weight_g, *weights.full_weight_g);
    } else if (weights.full_weight_g.has_value()) {
        add_fmt_item<N_("Full Weight"), "%.0f g"_tstr, float>(*weights.full_weight_g);
    }

    // !!! When adding new add_fmt_item calls, make sure that there's enough capacity in data_items_ to accomodate them all

    menu.index_mapping_.set_section_size<Item::data_section>(menu.data_items_.size());
    menu.setup_items();
    return true;
}

void ScreenOPTInfo::add_string_item(const char *label, std::string_view val, std::span<char> buffer) {
    // Copy the text to the provided buffer - the val likely comes from ReadFieldRequest, which is on the stack and will get destroyed
    const std::string_view cpy { buffer.data(), val.copy(buffer.data(), buffer.size()) };
    menu.menu.data_items_.push_back([label, cpy](ItemVariant &iv) {
        iv.emplace<WI_INFO_t>(_(label), cpy);
    });
}

template <auto label, auto fmt, typename... Args>
void ScreenOPTInfo::add_fmt_item(std::type_identity_t<Args>... args) {
    menu.menu.data_items_.push_back([args...](ItemVariant &iv) {
        WI_INFO_t::Buffer buf;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdouble-promotion"
        snprintf(buf.data(), buf.size(), fmt, args...);
#pragma GCC diagnostic pop
        iv.emplace<WI_INFO_t>(_(label), buf.data());
    });
}

ScreenFactory::Creator screen_opt_info_creator(VirtualToolIndex for_tool) {
    return ScreenFactory::ScreenWithArg<ScreenOPTInfo>(for_tool);
}

} // namespace buddy::openprinttag
