/// @file
#include "screen_menu_statistics.hpp"

#include <odometer.hpp>
#include <utils/string_builder.hpp>
#include <option/has_toolchanger.h>
#include <time_helper.hpp>
#include <img_resources.hpp>
#include <ScreenHandler.hpp>
#include <i_window_menu_item.hpp>
#include <window_menu_virtual.hpp>
#include <screen_menu.hpp>
#include <WindowMenuInfo.hpp>
#include <WindowMenuItems.hpp>
#include <dynamic_index_mapping.hpp>
#include <MItem_menus.hpp>

namespace {

string_view_utf8 physical_tool_item_title(uint8_t item_index, StringViewUtf8ParamBase &params) {
    return PhysicalToolIndex::from_raw(item_index).display_name(params);
}

struct Stat {
    using FmtF = void (*)(StringBuilder &sb);
    using ItemFmtF = void (*)(StringBuilder &sb, uint8_t item_index);
    using ItemTitleF = string_view_utf8 (*)(uint8_t item_index, StringViewUtf8ParamBase &params);

    /// Translatable string, label of the menu item
    const char *title;

    /// Formatable string to print the value
    FmtF fmt_f;

    uint8_t item_count = 1;
    ItemFmtF item_fmt_f = nullptr;
    ItemTitleF item_title_f = physical_tool_item_title;
};

void fmt_distance_mm(StringBuilder &sb, float distance_mm) {
    const float distance_m = distance_mm / 1000;
    if (distance_m >= 1000) {
        sb.append_printf("%.1f km", (double)(distance_m / 1000));
    } else {
        sb.append_printf("%.1f m", (double)distance_m);
    }
};

constexpr std::array stats {
    Stat {
        .title = N_("X Axis Travel"),
        .fmt_f = [](StringBuilder &sb) { fmt_distance_mm(sb, Odometer_s::instance().get_axis(Odometer_s::axis_t::X)); },
    },
        Stat {
            .title = N_("Y Axis Travel"),
            .fmt_f = [](StringBuilder &sb) { fmt_distance_mm(sb, Odometer_s::instance().get_axis(Odometer_s::axis_t::Y)); },
        },
        Stat {
            .title = N_("Z Axis Travel"),
            .fmt_f = [](StringBuilder &sb) { fmt_distance_mm(sb, Odometer_s::instance().get_axis(Odometer_s::axis_t::Z)); },
        },
        Stat {
            .title = N_("Extruded Filament"),
            .fmt_f = [](StringBuilder &sb) { fmt_distance_mm(sb, Odometer_s::instance().get_extruded_all()); },
            .item_count = PhysicalToolIndex::count,
            .item_fmt_f = [](StringBuilder &sb, uint8_t item_index) { fmt_distance_mm(sb, Odometer_s::instance().get_extruded(PhysicalToolIndex::from_raw(item_index))); },
        },
#if HAS_MMU2()
        Stat {
            .title = N_("MMU Filament Changes"),
            .fmt_f = [](StringBuilder &sb) { sb.append_printf("%" PRIu32, Odometer_s::instance().get_mmu_changes()); },
        },
#endif
#if HAS_TOOLCHANGER()
        Stat {
            .title = N_("Toolchanges"),
            .fmt_f = [](StringBuilder &sb) { sb.append_printf("%" PRIu32, Odometer_s::instance().get_toolpick_all()); },
            .item_count = PhysicalToolIndex::count,
            .item_fmt_f = [](StringBuilder &sb, uint8_t item_index) { sb.append_printf("%" PRIu32, Odometer_s::instance().get_toolpick(PhysicalToolIndex::from_raw(item_index))); },
        },
#endif
        Stat {
            .title = N_("Print Time"),
            .fmt_f = [](StringBuilder &sb) { format_duration(sb, Odometer_s::instance().get_time()); },
        },
};

enum class StatsItem {
    return_,
    fail_stats,
    stats_section,
};
constexpr auto stats_mapping_items = std::to_array<DynamicIndexMappingRecord<StatsItem>>({
    StatsItem::return_,
    StatsItem::fail_stats,
    { StatsItem::stats_section, DynamicIndexMappingType::static_section, stats.size() },
});
constexpr DynamicIndexMapping<stats_mapping_items> stats_mapping;

class MenuItemStatDetail final : public WI_INFO_t {
public:
    MenuItemStatDetail(const Stat &stat, uint8_t item_index)
        : WI_INFO_t(string_view_utf8 {}) {
        SetLabel(stat.item_title_f(item_index, title_params_));

        ArrayStringBuilder<GetInfoLen()> sb;
        stat.item_fmt_f(sb, item_index);
        ChangeInformation(sb.str());
    }

private:
    StringViewUtf8Parameters<4> title_params_;
};

class WindowMenuStatDetail final : public WindowMenuVirtual {
public:
    WindowMenuStatDetail(window_t *parent, Rect16 rect)
        : WindowMenuVirtual(parent, rect, CloseScreenReturnBehavior::yes) {
    }

    void set_stat(const Stat *stat) {
        stat_ = stat;
        setup_items();
    }

    int item_count() const override {
        // + 1 for MI_RETURN
        return stat_->item_count + 1;
    }

    void setup_item(ItemVariant &variant, int index) override {
        if (index == 0) {
            variant.emplace<MI_RETURN>();
        } else {
            variant.emplace<MenuItemStatDetail>(*stat_, index - 1);
        }
    }

private:
    const Stat *stat_ = nullptr;
};

class ScreenStatDetail final : public ScreenMenuBase<WindowMenuStatDetail> {
public:
    ScreenStatDetail(const Stat *stat)
        : ScreenMenuBase(nullptr, _(stat->title), EFooter::Off) {
        menu.menu.set_stat(stat);
    }
};

class MenuItemStats final : public WI_INFO_t {
public:
    MenuItemStats(const Stat &stat)
        : WI_INFO_t(_(stat.title))
        , stat_(stat) {
        ArrayStringBuilder<GetInfoLen()> sb;
        stat.fmt_f(sb);
        ChangeInformation(sb.str());

        if (stat.item_count > 1) {
            SetIconId(&img::arrow_right_10x16);
            set_icon_position(IconPosition::after_extension);
        }
    }

    void click(IWindowMenu &) override {
        if (stat_.item_count > 1) {
            Screens::Access()->Open(ScreenFactory::ScreenWithArg<ScreenStatDetail>(&stat_));
        }
    }

private:
    const Stat &stat_;
};

class WindowMenuStats final : public WindowMenuVirtual {
public:
    WindowMenuStats(window_t *parent, Rect16 rect)
        : WindowMenuVirtual(parent, rect, CloseScreenReturnBehavior::yes) {
        setup_items();
    }

    int item_count() const override {
        return stats_mapping.total_item_count();
    }

    void setup_item(ItemVariant &variant, int index) override {
        const auto mapping = stats_mapping.from_index(index);
        switch (mapping.item) {

        case StatsItem::return_:
            variant.emplace<MI_RETURN>();
            break;

        case StatsItem::fail_stats:
            variant.emplace<MI_FAIL_STAT>();
            break;

        case StatsItem::stats_section:
            variant.emplace<MenuItemStats>(stats[mapping.pos_in_section]);
            break;
        }
    }
};

class ScreenStats final : public ScreenMenuBase<WindowMenuStats> {
public:
    ScreenStats()
        : ScreenMenuBase(nullptr, _("STATISTICS"), EFooter::Off) {}
};

} // namespace

MI_STATISTICS::MI_STATISTICS()
    : MI_SCREEN_BASE(ScreenFactory::Screen<ScreenStats>, N_("Print Statistics")) {
}
