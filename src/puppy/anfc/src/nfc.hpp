#pragma once

#include <st25r39xxb/ST25R39XXB.hpp>
#include <prusa3d/nfc/request/debug/ModulationConfig_1_0.h>

namespace nfc {
extern st25r39xxb::ST25R39XXB reader_1;
void readers_init();
/// For debug pruposes ONLY
void reconfigure_readers(const prusa3d_nfc_request_debug_ModulationConfig_1_0 &config);
void irq();
} // namespace nfc
