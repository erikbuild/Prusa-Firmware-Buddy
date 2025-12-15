#include "screen_opt_settings.hpp"
#include "screen_opt_tag_list.hpp"

#include <screen_menu.hpp>
#include <ScreenHandler.hpp>

namespace buddy::openprinttag {

using ScreenOPTSettings_ = ScreenMenu<EFooter::Off,
    MI_RETURN,
    MI_OPT_TAG_LIST>;

class ScreenOPTSettings final : public ScreenOPTSettings_ {

public:
    ScreenOPTSettings()
        : ScreenOPTSettings_(_("OPENPRINTTAG SETTINGS")) {}
};

MI_OPT_SETTINGS::MI_OPT_SETTINGS()
    : IWindowMenuItem(_("OpenPrintTag")) {
    set_show_expand_icon();
}

void MI_OPT_SETTINGS::click(IWindowMenu &) {
    Screens::Access()->Open<ScreenOPTSettings>();
}

}; // namespace buddy::openprinttag
