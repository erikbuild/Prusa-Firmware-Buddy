#pragma once

#include <screen_menu.hpp>
#include <window_menu_virtual.hpp>

namespace screen_factory_reset {

class WindowMenuFactoryReset final : public WindowMenuVirtual {
public:
    WindowMenuFactoryReset(window_t *parent, Rect16 rect);

public:
    int item_count() const final;

protected:
    void setup_item(ItemVariant &variant, int index) final;
};

} // namespace screen_factory_reset

class ScreenFactoryReset final : public ScreenMenuBase<screen_factory_reset::WindowMenuFactoryReset> {
public:
    ScreenFactoryReset();
};
