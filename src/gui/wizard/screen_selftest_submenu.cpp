/// @file
#include "screen_selftest_submenu.hpp"

#include <tool_index.hpp>
#include <screen_menu_selftest_snake.hpp>

namespace SelftestSnake::screen_selftest_submenu {

Menu::Menu(window_t *parent, Rect16 rect)
    : WindowMenuVirtual(parent, rect, CloseScreenReturnBehavior::yes) {}

void Menu::setup(Action action) {
    action_ = action;
    setup_items();
}

int Menu::item_count() const {
    return PhysicalToolIndex::count + 1; // + MI_RETURN
}

void Menu::setup_item(ItemVariant &variant, int index) {
    if (index == 0) {
        variant.emplace<MI_RETURN>();
    } else {
        const auto tool = PhysicalToolIndex::from_raw(index - 1);
        variant.emplace<I_MI_STS_SUBMENU>(get_submenu_label_template(action_), action_, tool);
    }
}

Screen::Screen(Action action)
    : ScreenMenuBase(nullptr, _(get_action_label(action)), EFooter::Off)
    , action_(action) {
    menu.menu.setup(action);
}

void Screen::draw() {
    if (is_menu_draw_enabled(this)) {
        window_frame_t::draw();
    }
}

void Screen::windowEvent(window_t *sender, GUI_event_t event, void *param) {
    do_menu_event(this, sender, event, param, action_, true);
}

} // namespace SelftestSnake::screen_selftest_submenu
