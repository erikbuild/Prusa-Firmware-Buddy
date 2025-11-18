#include <otp.hpp>

#include <stm32c0xx_hal.h>

#include <cstring>
#include <string_view>

static constexpr uintptr_t OTP_START = 0x1FFF'7000;

static const auto otp = []() -> std::optional<OTP_v5> {
    auto *ptr = reinterpret_cast<uint8_t *>(OTP_START);
    // first byte is version of the structure (so 5)
    // if the otp was not burned (the value is 0xff),
    // then don't copy the otp here.
    if (*ptr == 0xFF) {
        return std::nullopt;
    }
    OTP_v5 o {};
    // We copy the otp structure to prevent problems with unalligned access to OTP flash
    std::memcpy(&o, ptr, sizeof(o));
    return { o };
}();

std::optional<board_revision_t> otp_get_board_revision() {
    return otp.transform([](const auto &o) {
        auto datamatrix = o.datamatrix;
        size_t i = 0;
        while (i < sizeof(datamatrix) && datamatrix[i] != '-') {
            ++i;
        }
        ++i;
        board_revision_t revision = 0;
        revision += (datamatrix[i] - '0') * 10;
        revision += (datamatrix[i + 1] - '0');
        return revision;
    });
}

std::optional<uint8_t> otp_get_bom_id() {
    return otp.transform([](const auto &o) { return o.bomID; });
}

uint32_t otp_get_timestamp() {
    return otp.transform([](const auto &o) { return o.timestamp; }).value_or(0);
}

uint8_t otp_get_serial_nr(serial_nr_t &sn) {
    if (!otp.has_value()) {
        return 0;
    }

    std::memcpy(sn.data(), otp->datamatrix, sizeof(otp->datamatrix));
    sn.at(sizeof(otp->datamatrix)) = '\0';

    return sizeof(OTP_v5::datamatrix) + 1;
}
