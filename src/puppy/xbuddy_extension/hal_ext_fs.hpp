/// @file
#pragma once

namespace hal::ext_fs {

/// Initialize EXT pin as 1-Wire master with up to two TMP1826 fsensors expanders
/// Also uses TIM6
void init();

/// To be called once on the HAL thread
void setup();

/// To be called on the HAL thread
void step();

} // namespace hal::ext_fs
