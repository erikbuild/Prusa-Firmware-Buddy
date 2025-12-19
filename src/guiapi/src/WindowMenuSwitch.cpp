#include "WindowMenuSwitch.hpp"

#include <string_builder.hpp>

MenuItemSwitch::MenuItemSwitch(const string_view_utf8 &label, const std::span<const char *const> &items, size_t initial_index)
    : MenuItemSelectMenu(label)
    , items_(items) //
{
    set_current_item(initial_index);
}

bool MenuItemSwitch::on_item_selected([[maybe_unused]] int old_index, [[maybe_unused]] int new_index) {
    OnChange(old_index);
    return true;
}

void MenuItemSwitch::build_item_text(int index, const std::span<char> &buffer) const {
    StringBuilder sb(buffer);
    const char *str = items_[index];
    if (translate_items_) {
        sb.append_string_view(_(str));
    } else {
        sb.append_string(str);
    }
}
