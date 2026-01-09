#pragma once
#include <gcode_info.hpp>
#include <window_text.hpp>
#include <window_icon.hpp>
#include <standard_frame/frame_prompt.hpp>
#include <fsm/print_preview_phases.hpp>

class FrameGCodeIncompatible : public FramePrompt {
public:
    FrameGCodeIncompatible(window_frame_t *parent, PhasesPrintPreview phase)
        : FramePrompt(parent, phase, {}) {}

    /* Will be completely rewritten in the next commit
        No point in wasting time making it work with the new system

private:
    struct Message {
        Message(window_t *parent, const string_view_utf8 &text, HWCheckSeverity severity, bool valid);
        Message(window_t *parent, const string_view_utf8 &text, const GCodeInfo::ValidPrinterSettings::Feature &feature);

        window_icon_t icon;
        window_text_t text;
    };
    const GCodeInfo::ValidPrinterSettings &valid_printer_settings;

    // Must be before messages!
    /// Max version len + some margin
    StringViewUtf8Parameters<sizeof(valid_printer_settings.latest_fw_version) + 5> wrong_fw_version_params;

    std::array<Message, hw_check_type_count + 3 + (HAS_MMU2() ? 1 : 0)> messages;

    Message unsupported_features;

    */
};
