#pragma once

#include <gui/menu_item/menu_item_select_menu.hpp>

class MenuItemSwitch : public MenuItemSelectMenu {

public:
    MenuItemSwitch(const string_view_utf8 &label, const std::span<const char *const> &items, size_t initial_index = 0);

    inline void set_translate_items(bool set) {
        translate_items_ = set;
    }

    inline int item_count() const final {
        return items_.size();
    }

    [[deprecated("Use MenuItemSelectMenu::current_item")]]
    inline size_t get_index() const {
        return current_item();
    }

protected:
    [[deprecated("Use on_item_selected")]]
    virtual void OnChange([[maybe_unused]] size_t old_index) {};

    bool on_item_selected([[maybe_unused]] int old_index, [[maybe_unused]] int new_index) override;

    void build_item_text(int index, const std::span<char> &buffer) const final;

private:
    std::span<const char *const> items_;
    bool translate_items_ = true;
};
