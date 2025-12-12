#pragma once

#include <filament.hpp>
#include <string_view_utf8.hpp>
#include <screen_menu.hpp>
#include <screen_fsm.hpp>
#include <filament_list.hpp>
#include <dynamic_index_mapping.hpp>
#include <window_menu_virtual.hpp>
#include <window_menu_callback_item.hpp>
#include <option/has_anfc.h>
#include <tool_index.hpp>

#include <MItem_tools.hpp>
#include <fsm_preheat_type.hpp>

namespace preheat_menu {

using PreheatToolIndex = PreheatData::ToolIndex;

class WindowMenuPreheat;

// extra space at the end is intended
class MI_FILAMENT : public WiInfo<sizeof("999/999 ")> {
public:
    MI_FILAMENT(FilamentType filament_type, PreheatToolIndex target_extruder);
    void click(IWindowMenu &) final;

    const FilamentType filament_type;
    const PreheatToolIndex tool;
    FilamentTypeParameters::Name filament_name;
};

#if HAS_ANFC()
class MI_FROM_OPENPRINTTAG : public IWindowMenuItem {
public:
    MI_FROM_OPENPRINTTAG(VirtualToolIndex tool);

    void click(IWindowMenu &) final;
    void Loop() final;

    const VirtualToolIndex tool_;
};

#endif

using WindowMenuPreheatBase = WindowMenuVirtual<
    WindowMenuCallbackItem,
#if HAS_ANFC()
    MI_FROM_OPENPRINTTAG,
#endif
    MI_FILAMENT>;

class WindowMenuPreheat : public WindowMenuPreheatBase {

public:
    WindowMenuPreheat(window_t *parent, const Rect16 &rect);

    void set_data(const PreheatData &data);
    void set_show_all_filaments(bool set);

    int item_count() const final {
        return index_mapping.total_item_count();
    }

    static bool handle_filament_selection(FilamentType filament_type, PreheatData::ToolIndex tool);

protected:
    void update_list();
    void setup_item(ItemVariant &variant, int index) final;

protected:
    void screenEvent(window_t *sender, GUI_event_t event, void *param) override;

private:
    enum class Item {
        return_,
#if HAS_ANFC()
        from_openprinttag,
#endif
        filament_section,
        show_all,
        cooldown,
        adhoc_filament,
    };

    static constexpr auto items = std::to_array<DynamicIndexMappingRecord<Item>>({
        { Item::return_, DynamicIndexMappingType::optional_item },
#if HAS_ANFC()
            { Item::from_openprinttag, DynamicIndexMappingType::optional_item },
#endif
            { Item::filament_section, DynamicIndexMappingType::dynamic_section },
            { Item::adhoc_filament },
            { Item::show_all, DynamicIndexMappingType::optional_item },
            { Item::cooldown, DynamicIndexMappingType::optional_item },
    });

private:
    FilamentList filament_list;
    DynamicIndexMapping<items> index_mapping;
    bool show_all_filaments_ = false;

    /// Extruder we're doing the load/preheat for
    PreheatToolIndex tool = AllTools {};
};

class ScreenPreheat : public ScreenFSM {

public:
    ScreenPreheat();
    ~ScreenPreheat();

protected:
    inline PhasesPreheat get_phase() const {
        return GetEnumFromPhaseIndex<PhasesPreheat>(fsm_base_data.GetPhase());
    }

protected:
    void create_frame();
    void destroy_frame();
    void update_frame();
};

}; // namespace preheat_menu

using ScreenPreheat = preheat_menu::ScreenPreheat;
