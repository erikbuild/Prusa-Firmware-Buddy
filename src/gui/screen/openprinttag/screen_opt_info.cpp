#include "screen_opt_info.hpp"
#include "screen_opt_info_private.hpp"

#include <window_msgbox.hpp>
#include <ScreenHandler.hpp>
#include <feature/openprinttag/requests_read_multi.hpp>
#include <feature/openprinttag/data_utils.hpp>
#include <string_builder.hpp>
#include <screen/openprinttag/opt_request_wizard.hpp>
#include <screen/filament/screen_filament_detail.hpp>

namespace buddy::openprinttag {

WindowMenuOPTInfo::WindowMenuOPTInfo(window_t *parent, Rect16 rect)
    : WindowMenuVirtual(parent, rect, CloseScreenReturnBehavior::yes) {
    setup_items();
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

    case Item::print_parameters:
        const auto callback = [this] {
            MsgBoxError(_("To unlock this functionality, you need to buy the Prusa: Tags & Spools Expansion Pack (not a DLC)."), Responses_Ok);
        };
        variant.emplace<WindowMenuCallbackItem>(_("Printing Parameters"), callback, nullptr, expands_t::yes);
        break;
    }
}

ScreenOPTInfo::ScreenOPTInfo(VirtualToolIndex tool)
    : ScreenMenuBase(nullptr, _("OPENPRINTTAG INFO"), EFooter::Off)
    , tool_(tool) {
    menu.menu.screen_ = this;

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
    tag_ = ToolTag::for_tool(tool_);
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
        std::string_view abbr = type.abbreviation;

        if (const auto default_abbr = req.result<MainField::material_type>().transform([](auto item) { return ::openprinttag::enum_item_name(item); }); default_abbr != type.abbreviation) {
            StringBuilder sb(abbreviation_buffer_);
            sb.append_printf("%.*s (%.*s)", abbr.size(), abbr.data(), default_abbr->size(), default_abbr->data());
            abbr = sb.str();
        }

        add_string_item(N_("Abbreviation"), abbr, abbreviation_buffer_);
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
