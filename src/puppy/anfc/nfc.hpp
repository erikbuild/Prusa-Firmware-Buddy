#pragma once

#include <st25r39xxb/ST25R39XXB.hpp>

namespace nfc {
extern st25r39xxb::ST25R39XXB reader_1;
void readers_init();
void irq();
} // namespace nfc
