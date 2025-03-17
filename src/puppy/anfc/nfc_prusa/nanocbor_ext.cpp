#include "nanocbor_ext.hpp"

int nanocbor_copy_value(nanocbor_value_t *src, nanocbor_encoder_t *tgt) {
    // Byte-copy the item that we read from source to target
    const uint8_t *data;
    size_t len;
    if (auto r = nanocbor_get_subcbor(src, &data, &len); r < 0) {
        return r;
    }

    if (!tgt->fits(tgt, tgt->context, len)) {
        return NANOCBOR_ERR_OVERFLOW;
    }

    tgt->append(tgt, tgt->context, data, len);
    return len;
}
