#include "screen_hw_mmu.hpp"

ScreenMenuHwMmu::ScreenMenuHwMmu()
    : ScreenMenu(string_view_utf8::MakeCPUFLASH("MMU")) {}
