/// @file
#include "hal_watchdog.hpp"

#include <stm32c0xx.h>

// Note: There is no initialization. Watchdog is initialized by bootloader.
//       If there is no bootloader, we do not get watchdog functionality.
//       This would only happen for users compiling their own firmware.
//       It is their responsibility to not burn anything.

void hal::watchdog::kick() {
    WRITE_REG(IWDG->KR, 0x0000aaaa);
}
