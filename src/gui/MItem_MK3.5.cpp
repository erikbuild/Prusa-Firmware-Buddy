#include "MItem_MK3.5.hpp"

MI_PINDA::MI_PINDA()
    : MenuItemAutoUpdatingLabel(string_view_utf8::MakeCPUFLASH("P.I.N.D.A."), "%i",
        [](auto) {
            return buddy::hw::zMin.read() == buddy::hw::Pin::State::low;
        } //
    ) {
}
