/// \file
#pragma once

#include <array>
#include <cstdint>

namespace can {

/// Traits modifier that modifies the base traits so that the message is stored in a serialized form
template <typename BaseTraits>
struct RawDataTraits : public BaseTraits {
    struct Type {
        std::array<uint8_t, BaseTraits::extent_bytes> serialized_data;
        size_t serialized_size = 0;
    };

    static inline int8_t serialize(const Type *obj, uint8_t *buffer, size_t *inout_buffer_size_bytes) {
        if (*inout_buffer_size_bytes < obj->serialized_size) {
            return -1;
        }

        memcpy(buffer, obj->serialized_data.data(), obj->serialized_size);
        *inout_buffer_size_bytes = obj->serialized_size;
        return 0;
    }

    static inline int8_t deserialize(Type *out, const uint8_t *data, size_t *bytes) {
        if (*bytes > BaseTraits::extent_bytes) {
            return -1;
        }

        memcpy(out->serialized_data.data(), data, *bytes);
        out->serialized_size = *bytes;
        return 0;
    }
};

struct RawPacketBaseTraits {
    static constexpr size_t data_size = 63; ///< Size of the data in the packet
    static constexpr size_t serialization_buffer_size_bytes = data_size;
    static constexpr size_t extent_bytes = data_size;
    static constexpr bool has_fixed_port_id = false;
};

/**
 * @brief Traits for raw one packet of data.
 * These allow using the 63 bytes of CANFD payload as is.
 * There is no dsdl type nor serialization, just 63 bytes of data.
 */
using RawPacketTraits = RawDataTraits<RawPacketBaseTraits>;

} // namespace can
