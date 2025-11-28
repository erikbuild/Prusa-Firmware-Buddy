#pragma once

#include <filament.hpp>

#include <WindowMenuInfo.hpp>
#include <WindowMenuSpin.hpp>
#include <MItem_tools.hpp>
#include <screen_menu.hpp>
#include <numeric_input_config.hpp>
#include <tool_index.hpp>

#include <option/has_chamber_api.h>
#include <option/has_filament_heatbreak_param.h>

namespace screen_filament_detail {

class MI_FILAMENT_NAME final : public WiInfo<32> {
public:
    static constexpr auto parameter_ptr = &FilamentTypeParameters::name;

    MI_FILAMENT_NAME();

    void set_filament_type(FilamentType set);

    const auto &value() const {
        return value_;
    }

    void set_value(const FilamentTypeParameters::Name &set);

    void click(IWindowMenu &) override;

    void update_text();

protected:
    FilamentTypeParameters::Name value_;
    FilamentType filament_type_;
};

class MI_FILAMENT_NOZZLE_TEMPERATURE final : public WiSpin {
public:
    static constexpr auto parameter_ptr = &FilamentTypeParameters::nozzle_temperature;

    MI_FILAMENT_NOZZLE_TEMPERATURE();
};

class MI_FILAMENT_NOZZLE_PREHEAT_TEMPERATURE final : public WiSpin {
public:
    static constexpr auto parameter_ptr = &FilamentTypeParameters::nozzle_preheat_temperature;

    MI_FILAMENT_NOZZLE_PREHEAT_TEMPERATURE();
};

class MI_FILAMENT_BED_TEMPERATURE final : public WiSpin {
public:
    static constexpr auto parameter_ptr = &FilamentTypeParameters::heatbed_temperature;

    MI_FILAMENT_BED_TEMPERATURE();
};

#if HAS_FILAMENT_HEATBREAK_PARAM()
class MI_FILAMENT_HEATBREAK_TEMPERATURE final : public WiSpin {
public:
    static constexpr auto parameter_ptr = &FilamentTypeParameters::heatbreak_temperature;

    MI_FILAMENT_HEATBREAK_TEMPERATURE();
};
#endif

#if HAS_CHAMBER_API()
class MI_FILAMENT_MIN_CHAMBER_TEMPERATURE final : public WiSpin {
public:
    static constexpr auto parameter_ptr = &FilamentTypeParameters::chamber_min_temperature;
    static constexpr bool is_chamber_item = true;

    MI_FILAMENT_MIN_CHAMBER_TEMPERATURE();
};
#endif

#if HAS_CHAMBER_API()
class MI_FILAMENT_MAX_CHAMBER_TEMPERATURE final : public WiSpin {
public:
    static constexpr auto parameter_ptr = &FilamentTypeParameters::chamber_max_temperature;
    static constexpr bool is_chamber_item = true;

    MI_FILAMENT_MAX_CHAMBER_TEMPERATURE();
};
#endif

#if HAS_CHAMBER_API()
class MI_FILAMENT_TARGET_CHAMBER_TEMPERATURE final : public WiSpin {
public:
    static constexpr auto parameter_ptr = &FilamentTypeParameters::chamber_target_temperature;
    static constexpr bool is_chamber_item = true;

    MI_FILAMENT_TARGET_CHAMBER_TEMPERATURE();
};
#endif

#if HAS_CHAMBER_API()
class MI_FILAMENT_REQUIRES_FILTRATION final : public WI_ICON_SWITCH_OFF_ON_t {
public:
    static constexpr auto parameter_ptr = &FilamentTypeParameters::requires_filtration;

    MI_FILAMENT_REQUIRES_FILTRATION();
};
#endif

class MI_FILAMENT_IS_ABRASIVE final : public WI_ICON_SWITCH_OFF_ON_t {
public:
    static constexpr auto parameter_ptr = &FilamentTypeParameters::is_abrasive;

    MI_FILAMENT_IS_ABRASIVE();
};

class MI_FILAMENT_DO_NOT_AUTO_RETRACT final : public WI_ICON_SWITCH_OFF_ON_t {
public:
    static constexpr auto parameter_ptr = &FilamentTypeParameters::do_not_auto_retract;

    MI_FILAMENT_DO_NOT_AUTO_RETRACT();
};

class MI_FILAMENT_VISIBLE final : public WI_ICON_SWITCH_OFF_ON_t {
public:
    MI_FILAMENT_VISIBLE();
};

class MI_CONFIRM final : public IWindowMenuItem {
public:
    MI_CONFIRM();
    void click(IWindowMenu &) override;
    stdext::inplace_function<void()> callback;
};

using ScreenFilamentDetail_ = ScreenMenu<EFooter::Off,
    MI_RETURN,
    MI_FILAMENT_NAME,
    MI_FILAMENT_VISIBLE,
    MI_FILAMENT_NOZZLE_TEMPERATURE,
    MI_FILAMENT_NOZZLE_PREHEAT_TEMPERATURE,
    MI_FILAMENT_BED_TEMPERATURE,
#if HAS_FILAMENT_HEATBREAK_PARAM()
    MI_FILAMENT_HEATBREAK_TEMPERATURE,
#endif
#if HAS_CHAMBER_API()
    MI_FILAMENT_TARGET_CHAMBER_TEMPERATURE,
    MI_FILAMENT_MIN_CHAMBER_TEMPERATURE,
    MI_FILAMENT_MAX_CHAMBER_TEMPERATURE,
#endif
    MI_FILAMENT_IS_ABRASIVE,
    MI_FILAMENT_DO_NOT_AUTO_RETRACT,
#if HAS_CHAMBER_API()
    MI_FILAMENT_REQUIRES_FILTRATION,
#endif
    MI_CONFIRM //
    >;

/// Management of a specified filament type
class ScreenFilamentDetail : public ScreenFilamentDetail_ {
public:
    /// When the detail screen is opened from within the preheat menu.
    /// Adds a "Confirm" button that sends the filament as a response to the preheat FSM
    struct PreheatModeParams {
        uint8_t target_extruder = 0;
    };

public:
    ScreenFilamentDetail(FilamentType filament_type);

    /// Shows the screen in the PendingAdHocFilament mode for preheat
    /// The added "Confirm" button sends the response to the Preheat FSM
    ScreenFilamentDetail(PreheatModeParams preheat_mode);

    ~ScreenFilamentDetail();

public:
    /// Fills the screen with the provided data
    void setup(FilamentType filament_type, const FilamentTypeParameters &params);
    void setup(FilamentType filament_type);

    void save_changes();

protected:
    ScreenFilamentDetail(const char *title);

    void setup_preheat_mode_confirm(VirtualToolIndex tool);

protected:
    FilamentType filament_type_;
};

}; // namespace screen_filament_detail

using ScreenFilamentDetail = screen_filament_detail::ScreenFilamentDetail;
