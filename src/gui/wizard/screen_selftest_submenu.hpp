/// @file
#pragma once

#include <window_menu_virtual.hpp>
#include <selftest_snake_config.hpp>
#include <screen_menu.hpp>

namespace SelftestSnake::screen_selftest_submenu {

class Menu final : public WindowMenuVirtual {

public:
    Menu(window_t *parent, Rect16 rect);

    void setup(Action action);

    int item_count() const override;

protected:
    void setup_item(ItemVariant &variant, int index) override;

private:
    /// MUST be set by calling setup()
    Action action_ = Action::_count;
};

class Screen final : public ScreenMenuBase<Menu> {

public:
    Screen(Action action);

protected:
    // Some legacy snake BS
    virtual void draw() override;
    virtual void windowEvent(window_t *sender, GUI_event_t event, void *param) override;

private:
    const Action action_;
};

}; // namespace SelftestSnake::screen_selftest_submenu

using ScreenSelftestSubmenu = SelftestSnake::screen_selftest_submenu::Screen;
