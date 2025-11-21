#include "frame_invalid_printer.hpp"

#include "img_resources.hpp"
#include <guiconfig/guiconfig.h>
#include <option/has_mmu2.h>
#include <find_error.hpp>
#include <fsm/print_preview_mapper.hpp>
#include <utils/string_builder.hpp>
#include <fonts.hpp>

FrameInvalidPrinter::Message::Message(window_t *parent, const string_view_utf8 &text, HWCheckSeverity severity, bool valid)
    : icon(parent, {}, (severity == HWCheckSeverity::Abort) ? &img::nok_16x16 : &img::warning_16x16)
    , text(parent, {}, is_multiline::yes, is_closed_on_click_t::no, text) {
    if (valid) {
        icon.Hide();
        this->text.Hide();
    }
}

FrameInvalidPrinter::Message::Message(window_t *parent, const string_view_utf8 &text, const GCodeInfo::ValidPrinterSettings::Feature &feature)
    : Message(parent, text, feature.get_severity(), feature.is_valid()) {}

FrameInvalidPrinter::FrameInvalidPrinter(window_frame_t *parent, PhasesPrintPreview phase)
    : FramePrompt(parent, phase, map_print_preview_phase_to_error_code)
    , valid_printer_settings(GCodeInfo::getInstance().get_valid_printer_settings())
    , messages({
        { parent, _("Printer doesn't have enough tools"), valid_printer_settings.wrong_tools },
            { parent, _("Nozzle diameter doesn't match"), valid_printer_settings.wrong_nozzle_diameter },
            { parent, _("Nozzle is not hardened"), valid_printer_settings.nozzle_not_hardened },
            { parent, _("Nozzle is not high-flow"), valid_printer_settings.nozzle_not_high_flow },
            { parent, _("Printer model doesn't match"), valid_printer_settings.wrong_printer_model },
            { parent, _("G-code version doesn't match"), valid_printer_settings.wrong_gcode_level },
#if HAS_GCODE_COMPATIBILITY()
            { parent, _("G-code compatibility mode"), valid_printer_settings.gcode_compatibility_mode },
#endif
            { parent,
                (HAS_LARGE_DISPLAY() ? _("Newer firmware is required: %s") : _("Newer FW req.: %s"))
                    .formatted(wrong_fw_version_params, valid_printer_settings.latest_fw_version),
                valid_printer_settings.wrong_firmware },
#if HAS_MMU2()
            { parent, _("Nozzle flow rate doesn't match"), valid_printer_settings.nozzle_flow_mismatch },
#endif
            { parent, _("G-code not sliced for input shaping"), valid_printer_settings.sliced_without_input_shaper },
    })
    , unsupported_features(parent,
          (HAS_LARGE_DISPLAY() ? _("Following features are required:") : _("Features required:")),
          HWCheckSeverity::Abort, !valid_printer_settings.unsupported_features)
    , unsupported_features_text(parent, {}, is_multiline::no) {

    static constexpr const Rect16::Width_t icon_margin = GuiDefaults::InvalidPrinterIconMargin;
    static constexpr const Rect16::Height_t line_spacing = GuiDefaults::InvalidPrinterLineSpacing;
    static constexpr const Rect16::Width_t img_w = img::warning_16x16.w;
    static constexpr const Rect16::Height_t img_h = img::warning_16x16.h;

    Rect16::Height_t h = resource_font(info.get_font())->h;

    Rect16 icon_rect = { info.GetRect().TopLeft(), img_w, img_h };

#if HAS_MINI_DISPLAY()
    Rect16::Height_t item_h = (std::min(h, img_h) + line_spacing) * 2;
    Rect16 text_rect = info.GetRect() = Rect16::Height_t(2 * item_h);
#elif HAS_LARGE_DISPLAY()
    Rect16::Height_t item_h = std::min(h, img_h) + line_spacing;
    Rect16 text_rect = info.GetRect() = Rect16::Height_t(h);
#endif
    info.SetRect(text_rect);

#if HAS_LARGE_DISPLAY()
    // Make a separator empty line only if there is room for it
    auto lines = std::count_if(begin(messages), end(messages), [](auto &m) { return m.text.HasVisibleFlag(); }) + (unsupported_features.text.HasVisibleFlag() ? 2 : 0);
    if (lines <= 6) {
        icon_rect += Rect16::Top_t(item_h);
        text_rect += Rect16::Top_t(item_h);
    }
#endif
    text_rect += Rect16::Left_t(img_w + icon_margin);
    text_rect -= Rect16::Width_t(img_w + icon_margin);

    for (auto &m : messages) {
        if (m.text.HasVisibleFlag()) {
            icon_rect += Rect16::Top_t(item_h);
            text_rect += Rect16::Top_t(item_h);
            m.icon.SetRect(icon_rect);
            m.text.SetRect(text_rect);
        }
    }

    // Show unsupported features
    if (unsupported_features.text.HasVisibleFlag()) {
        icon_rect += Rect16::Top_t(item_h);
        text_rect += Rect16::Top_t(item_h);
        unsupported_features.icon.SetRect(icon_rect);
        unsupported_features.text.SetRect(text_rect);
        text_rect += Rect16::Top_t(item_h);
        text_rect += Rect16::Left_t(10);
        unsupported_features_text.SetText(string_view_utf8::MakeRAM(valid_printer_settings.unsupported_features_text));
        unsupported_features_text.SetRect(text_rect);
    } else {
        unsupported_features_text.Hide();
    }
}
