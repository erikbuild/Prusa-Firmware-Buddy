#include "crc32.h"
#include "crc32.hpp"

extern uint32_t crc32_sw(const uint8_t *buffer, uint32_t length, uint32_t crc) {
    uint32_t value = crc ^ 0xFFFFFFFF;
    while (length--) {
        value ^= (uint32_t)*buffer++;
        for (int bit = 0; bit < 8; bit++) {
            if (value & 1) {
                value = (value >> 1) ^ 0xEDB88320;
            } else {
                value >>= 1;
            }
        }
    }
    value ^= 0xFFFFFFFF;
    return value;
}
uint32_t crc32_calc(const uint8_t *data, uint32_t count) {
    return crc32_calc_ex(0, data, count);
}
extern uint32_t crc32_calc_ex(uint32_t crc, const uint8_t *data, uint32_t count) {
    // use the software implementation to calculate the rest
    crc = crc32_sw(data, count, crc);

    return crc;
}

uint32_t crc32(uint32_t crc, Bytes data) {
    return crc32_calc_ex(crc, reinterpret_cast<const uint8_t *>(data.data()), data.size());
}
