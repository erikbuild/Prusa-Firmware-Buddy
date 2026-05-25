#pragma once

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

/// Initialize the CRC peripheral and associated mutex
extern void crc32_init(void);

/// Calculate reverse CRC32 for a given buffer
/// @param count size of @data in bytes
extern uint32_t crc32_calc(const uint8_t *data, uint32_t count);

/// Calculate reverse CRC32 for a given buffer with an explicit initial CRC value
extern uint32_t crc32_calc_ex(uint32_t crc, const uint8_t *data, uint32_t count);

/// Calculate reverse CRC32 for a given buffer with an explicit initial CRC value with same polynomial as Zlib
extern uint32_t crc32_sw(const uint8_t *buffer, uint32_t length, uint32_t crc);

#ifdef __cplusplus
}
#endif //__cplusplus
