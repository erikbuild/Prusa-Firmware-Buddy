/// @file
#include "hal_rng.hpp"

#include <stm32h5xx_ll_rng.h>

static void rng_recovery() {
    if (LL_RNG_IsActiveFlag_SECS(RNG)) {
        // Sequence to fully recover from a seed error
        LL_RNG_EnableCondReset(RNG);
        LL_RNG_DisableCondReset(RNG);
        while (LL_RNG_IsEnabledCondReset(RNG)) {
        }
        if (LL_RNG_IsActiveFlag_SEIS(RNG)) {
            LL_RNG_ClearFlag_SEIS(RNG);
        }
        while (LL_RNG_IsActiveFlag_SECS(RNG)) {
        }
    } else {
        // peripheral performed the reset automatically
        LL_RNG_ClearFlag_SEIS(RNG);
    }
}

void hal::rng::init() {
    __HAL_RCC_RNG_CONFIG(RCC_RNGCLKSOURCE_PLL1Q);
    __HAL_RCC_RNG_CLK_ENABLE();
    LL_RNG_Disable(RNG);
    LL_RNG_EnableClkErrorDetect(RNG);
    LL_RNG_SetHealthConfig(RNG, RNG_HTCR_NIST_VALUE);
    LL_RNG_EnableNistCompliance(RNG);
    while (LL_RNG_IsEnabledCondReset(RNG)) {
    }
    LL_RNG_Enable(RNG);
}

uint32_t hal::rng::get() {
    // Note: RNG functionality is critical for correct operation of xbuddy extension.
    //       If we ever hang in these loops, parent system will restart us after a while.
    for (;;) {
        // check if there is a seed error
        if (LL_RNG_IsActiveFlag_SEIS(RNG)) {
            rng_recovery();
        }

        // wait for random data
        while (!LL_RNG_IsActiveFlag_DRDY(RNG)) {
        }
        const uint32_t result = LL_RNG_ReadRandData32(RNG);

        if (LL_RNG_IsActiveFlag_SEIS(RNG)) {
            // not enough entropy, try again
            continue;
        } else {
            return result;
        }
    }
}
