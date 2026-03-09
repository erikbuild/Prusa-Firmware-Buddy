#include "screen_opt_tag_list.hpp"
#include "screen_opt_info.hpp"

#include <inplace_vector.hpp>

#include <WindowMenuItems.hpp>
#include <ScreenHandler.hpp>
#include <screen_menu.hpp>
#include <window_menu_virtual.hpp>
#include <dynamic_index_mapping.hpp>

#include <feature/openprinttag/tool_tag.hpp>

namespace buddy::openprinttag {

namespace {
    enum class Item {
        return_,
        tools_section,
    };

    static constexpr auto index_mapping_items = std::to_array<DynamicIndexMappingRecord<Item>>({
        Item::return_,
        { Item::tools_section, DynamicIndexMappingType::dynamic_section },
    });

    class MI_OPT_TOOL_TAG final : public IWindowMenuItem {

    public:
        MI_OPT_TOOL_TAG(VirtualToolIndex tool)
            : tool_(tool) {
            SetLabel(tool.display_name(label_params_));
            set_show_expand_icon();
        }

        void click(IWindowMenu &) override {
            // Open the screen even if the slot is "disabled"
            // The screen will show an error and close if there is no tag detected
            Screens::Access()->Open(screen_opt_info_creator(tool_));
        }

        void Loop() override {
            // Indicate the item as disabled, but do not really disable it
            // This is so that the click() function gets called even if the item is disabled
            set_color_scheme(ToolTag::for_tool_ephemeral(tool_).has_value() ? &color_scheme_default : &color_scheme_default_disabled);
        }

    private:
        const VirtualToolIndex tool_;
        VirtualToolIndex::DisplayNameParams label_params_;
    };

} // namespace
class WindowMenuOPTTagList final : public WindowMenuVirtual {

public:
    WindowMenuOPTTagList(window_t *parent, Rect16 rect)
        : WindowMenuVirtual(parent, rect, CloseScreenReturnBehavior::yes) {

        for (VirtualToolIndex tool : VirtualToolIndex::all().skip_all_disabled()) {
            enabled_tools_.push_back(tool);
        }

        index_mapping_.set_section_size<Item::tools_section>(enabled_tools_.size());

        setup_items();
    }

    int item_count() const final {
        return index_mapping_.total_item_count();
    }

protected:
    void setup_item(ItemVariant &variant, int index) final {
        const auto mapping = index_mapping_.from_index(index);
        switch (mapping.item) {

        case Item::return_:
            variant.emplace<MI_RETURN>();
            break;

        case Item::tools_section:
            variant.emplace<MI_OPT_TOOL_TAG>(*enabled_tools_[mapping.pos_in_section]);
            break;
        }
    }

private:
    DynamicIndexMapping<index_mapping_items> index_mapping_;

    // Note: optional necessary because stdext::inplace_vector doesn't play well with non-default-constructible types, which ToolIndex is
    stdext::inplace_vector<std::optional<VirtualToolIndex>, VirtualToolIndex::count> enabled_tools_;
};

class ScreenOPTTagList final : public ScreenMenuBase<WindowMenuOPTTagList> {

public:
    ScreenOPTTagList()
        : ScreenMenuBase(nullptr, _("OPENPRINTTAG LIST"), EFooter::Off) {}
};

MI_OPT_TAG_LIST::MI_OPT_TAG_LIST()
    : IWindowMenuItem(_("Tag List")) {
    set_show_expand_icon();
}

void MI_OPT_TAG_LIST::click(IWindowMenu &) {
    Screens::Access()->Open<ScreenOPTTagList>();
}
}; // namespace buddy::openprinttag
