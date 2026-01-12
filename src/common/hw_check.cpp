#include <common/hw_check.hpp>

#include <img_resources.hpp>

constinit const EnumArray<HWCheckType, const char *, hw_check_type_count> hw_check_type_names {
    { HWCheckType::nozzle, N_("Nozzle") },
        { HWCheckType::model, N_("Printer Model") },
        { HWCheckType::firmware, N_("Firmware Version") },
#if HAS_GCODE_COMPATIBILITY()
        { HWCheckType::gcode_compatibility, N_("G-Code Compatibility") },
#endif
        { HWCheckType::gcode_level, N_("G-Code Level") },
        { HWCheckType::input_shaper, N_("Input Shaper") },
};

constinit const EnumArray<HWCheckSeverity, const img::Resource *, std::to_underlying(HWCheckSeverity::_last) + 1> hw_check_severity_icons {
    { HWCheckSeverity::Ignore, nullptr },
    { HWCheckSeverity::Warning, &img::warning_16x16 },
    { HWCheckSeverity::Abort, &img::nok_color_16x16 },
};
