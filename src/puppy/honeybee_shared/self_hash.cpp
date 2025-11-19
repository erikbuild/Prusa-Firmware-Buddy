
#include "self_hash.hpp"

#include <device/hal.h>
#include <device/peripherals.h>

#ifndef HASH_ALGOSELECTION_SHA256
    #include <mbedtls/sha256.h>
#endif /* HASH_ALGOSELECTION_SHA256 */

// Linker script constants
extern const uint32_t __fw_descriptor_start;
extern const uint32_t __fw_descriptor_length;
extern const uint32_t __flash_start;

bool self_hash_get(uint32_t salt, uint8_t output[32]) {
    bool ret = false;

    // Get the application area including the descriptor
    const uint8_t *APP_START = reinterpret_cast<const uint8_t *>(&__flash_start);
    const size_t APP_SIZE = reinterpret_cast<const uint8_t *>(&__fw_descriptor_start)
        - reinterpret_cast<const uint8_t *>(&__flash_start)
        + reinterpret_cast<size_t>(&__fw_descriptor_length);

#ifdef HASH_ALGOSELECTION_SHA256

    // Use hardware sha when available
    ret = (HAL_HASH_Accumulate(&hhash, reinterpret_cast<uint8_t *>(&salt), sizeof(salt), HAL_MAX_DELAY) == HAL_OK);
    ret &= (HAL_HASH_AccumulateLast(&hhash, APP_START, APP_SIZE, output, HAL_MAX_DELAY) == HAL_OK);

#else /* HASH_ALGOSELECTION_SHA256 */

    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    do {
        if (mbedtls_sha256_starts_ret(&ctx, false) != 0) {
            break;
        }
        if (mbedtls_sha256_update_ret(&ctx, reinterpret_cast<uint8_t *>(&salt), sizeof(salt)) != 0) {
            break;
        }
        if (mbedtls_sha256_update_ret(&ctx, APP_START, APP_SIZE) != 0) {
            break;
        }
        if (mbedtls_sha256_finish_ret(&ctx, output) != 0) {
            break;
        }
        ret = true;
    } while (0);
    mbedtls_sha256_free(&ctx);

#endif /* HASH_ALGOSELECTION_SHA256 */

    return ret;
}
