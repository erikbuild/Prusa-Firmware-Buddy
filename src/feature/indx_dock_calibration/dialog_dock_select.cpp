/// @file
#include "dialog_dock_select.hpp"

#include <bitset>
#include <cstdio>
#include <window_menu_adv.hpp>
#include <window_menu_virtual.hpp>
#include <WindowMenuItems.hpp>
#include <img_resources.hpp>
#include <WindowMenuSwitch.hpp>
#include <ScreenHandler.hpp>
#include <window_header.hpp>
#include <dynamic_index_mapping.hpp>
#include <config_store/store_instance.hpp>
#include <guiconfig/GuiDefaults.hpp>
#include <tool_index.hpp>
#include <i18n.h>

namespace {

using DockMask = std::bitset<PhysicalToolIndex::count>;

enum class Item {
    return_,
    dock,
    continue_,
};

static constexpr auto index_mapping_items = std::to_array<DynamicIndexMappingRecord<Item>>({
    { Item::return_ },
    { Item::dock, DynamicIndexMappingType::dynamic_section },
    { Item::continue_ },
});

class SelectDocksMenu;

static constexpr const char *dock_toggle_items[] = { N_("No"), N_("Yes") };

class MI_DOCK_TOGGLE final : public MenuItemSwitch {

public:
    MI_DOCK_TOGGLE(SelectDocksMenu &menu, PhysicalToolIndex tool, bool already_calibrated, bool selected)
        : MenuItemSwitch(string_view_utf8 {}, dock_toggle_items, selected ? 1 : 0)
        , menu_(menu)
        , dock_(tool) {
        set_behavior(Behavior::quick_cycle);
        snprintf(label_buffer_, sizeof(label_buffer_), "Dock %d", tool.to_raw() + 1);
        SetLabel(string_view_utf8::MakeRAM(reinterpret_cast<const uint8_t *>(label_buffer_)));
        SetIconId(already_calibrated ? &img::ok_color_16x16 : &img::nok_color_16x16);
    }

protected:
    void OnChange(size_t) override;

    void printExtension(Rect16 extension_rect, [[maybe_unused]] Color color_text, Color color_back, ropfn raster_op) const override {
        MenuItemSwitch::printExtension(extension_rect, current_item() ? COLOR_GREEN : COLOR_RED, color_back, raster_op);
    }

private:
    SelectDocksMenu &menu_;
    const PhysicalToolIndex dock_;
    char label_buffer_[16] {};
};

class MI_CONTINUE final : public IWindowMenuItem {

public:
    MI_CONTINUE(SelectDocksMenu &menu)
        : IWindowMenuItem(_(label))
        , menu_(menu) {}

    static constexpr const char *label = N_("Continue");

protected:
    void click(IWindowMenu &) override;

private:
    SelectDocksMenu &menu_;
};

class SelectDocksMenu : public WindowMenuVirtual {
    friend class MI_DOCK_TOGGLE;
    friend class MI_CONTINUE;

public:
    SelectDocksMenu(window_frame_t *parent, Rect16 rect, uint8_t dock_count)
        : WindowMenuVirtual(parent, rect, CloseScreenReturnBehavior::yes)
        , calibrated_docks(config_store().indx_dock_calibrated_mask.get())
        , dock_count_(dock_count) {

        // Pre-select only uncalibrated docks
        for (uint8_t i = 0; i < dock_count_; i++) {
            if (!calibrated_docks.test(i)) {
                selected_docks.set(i);
            }
        }

        index_mapping_.set_section_size<Item::dock>(dock_count_);
        setup_items();
    }

    int item_count() const final {
        return index_mapping_.total_item_count();
    }

    void setup_item(ItemVariant &variant, int index) final {
        const auto m = index_mapping_.from_index(index);

        switch (m.item) {

        case Item::return_:
            variant.emplace<MI_RETURN>();
            break;

        case Item::dock: {
            const auto dock_index = nth_dock(m.pos_in_section);
            variant.emplace<MI_DOCK_TOGGLE>(*this, dock_index, calibrated_docks.test(dock_index.to_raw()), selected_docks.test(dock_index.to_raw()));
            break;
        }

        case Item::continue_:
            variant.emplace<MI_CONTINUE>(*this);
            break;
        }
    }

    DockMask selected_docks;

    /// Result: nullopt = aborted, otherwise bitmask as uint8_t for FSMResponseVariant transfer
    std::optional<uint8_t> result;

private:
    /// Get the dock index for the nth dock in the list
    PhysicalToolIndex nth_dock(int n) const {
        return PhysicalToolIndex::from_raw(n);
    }

    const DockMask calibrated_docks;
    const uint8_t dock_count_;
    DynamicIndexMapping<index_mapping_items> index_mapping_;
};

void MI_DOCK_TOGGLE::OnChange(size_t) {
    menu_.selected_docks.set(dock_.to_raw(), current_item() == 1);
}

void MI_CONTINUE::click(IWindowMenu &) {
    menu_.result = static_cast<uint8_t>(menu_.selected_docks.to_ulong());
    Screens::Access()->Close();
}

class SelectDocksDialog final : public IDialog {

public:
    SelectDocksDialog(uint8_t dock_count)
        : header_(this)
        , menu_(this, GuiDefaults::RectScreenNoHeader, dock_count) {
        header_.SetText(_("SELECT DOCKS"));
        CaptureNormalWindow(menu_);
    }

    auto result() const {
        return menu_.menu.result;
    }

private:
    window_header_t header_;
    WindowExtendedMenu<SelectDocksMenu> menu_;
};

} // namespace

std::optional<uint8_t> select_docks_dialog(uint8_t dock_count) {
    SelectDocksDialog d(dock_count);
    Screens::Access()->gui_loop_until_dialog_closed();
    return d.result();
}
