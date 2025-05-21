#pragma once

#include <cstdint>

/**
 * @brief Get the firmware salted hash.
 * The hash is done over the application area, including the firmware descriptor.
 * @param[in] salt given to sha256 before the firmware data
 * @param[out] output 32 bytes of the hash
 * @return true if the hash was calculated
 */
bool self_hash_get(uint32_t salt, uint8_t output[32]);
