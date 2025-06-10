#include <st25r39xxb/ST25R39XXB.hpp>
#include <st25r39xxb/ST25R39XXB_defs.hpp>
#include <cassert>

#include <raii/scope_guard.hpp>
#include <iso13239/crc.hpp>
#include <inplace_vector.hpp>
#include <nfcv/encode.hpp>
#include <nfcv/decode.hpp>

static constexpr nfcv::Error convert_error(st25r39xxb::IRQType error_irqs) {
    using namespace st25r39xxb;
    if (std::to_underlying(error_irqs & IRQType::hard_framing_error)) {
        return nfcv::Error::device_hard_framing_error;
    } else if (std::to_underlying(error_irqs & IRQType::soft_framing_error)) {
        return nfcv::Error::device_soft_framing_error;
    } else if (std::to_underlying(error_irqs & IRQType::parity_error)) {
        return nfcv::Error::device_parity_error;
    } else if (std::to_underlying(error_irqs & IRQType::crc_error)) {
        return nfcv::Error::device_crc_error;
    }
    std::abort();
}

nfcv::Result<void> st25r39xxb::ST25R39XXB::await_interrupt(st25r39xxb::IRQType irqs_to_wait_for, uint32_t timeout_ms) {
    using namespace st25r39xxb;
    while (true) {
        timeout_ms = sys_int.await_interrupt(timeout_ms);
        const auto irqs = read_interrupt();
        if (std::to_underlying(irqs & IRQType::errors)) {
            return std::unexpected(convert_error(irqs));
        }
        irqs_to_wait_for = irqs_to_wait_for & (~irqs);
        if (std::to_underlying(irqs_to_wait_for) == 0) {
            break;
        }
        if (timeout_ms == 0) {
            return std::unexpected(nfcv::Error::timeout);
        }
    }

    return {};
}

st25r39xxb::IRQType st25r39xxb::ST25R39XXB::read_interrupt() {
    static_assert(std::endian::native == std::endian::little);
    IRQType res;
    hw_int.read_registers_continuous(RegisterA::main_interrupt, std::span { reinterpret_cast<std::byte *>(&res), sizeof(res) });
    return res;
}

void st25r39xxb::ST25R39XXB::set_interrupt_mask(st25r39xxb::IRQType mask) {
    static_assert(std::endian::native == std::endian::little);
    hw_int.write_registers_continuous(RegisterA::mask_main_interrupt, std::span { reinterpret_cast<std::byte *>(&mask), sizeof(mask) });
}

nfcv::Result<void> st25r39xxb::ST25R39XXB::init() {
    hw_int.direct_command(Command::set_default_2);

    hw_int.write_register(RegisterA::io_configuration_2, std::byte { 0x04 }); // IO configuration register 2

    /*auto version = TRY(read_register(RegisterA::ICIdentity));
    static constexpr std::byte TYPE_CODE_MASK { 0b0001'1111 };
    static constexpr std::byte ST25R3916B_ID { 0b0000'1100 };
    if ((version & TYPE_CODE_MASK) != ST25R3916B_ID) {
        return std::unexpected(Error::InvalidChip);
    }*/

    // Disable IRQs
    set_interrupt_mask(IRQType::all);
    // Clear IRQs
    [[maybe_unused]] const auto v = read_interrupt();
    // Flipper here disables overheat protection - we probably don't have this problem so we don't need to do it.
    // But it might be problem in the future. Also It uses undocumented register to do so
    // change_register(static_cast<RegisterB>(0x04), std::byte { 0x10 }, std::byte { 0x10 });

    // Enable internal oscilator
    auto res = turn_on_oscilator();
    if (!res.has_value()) {
        return res;
    }

    // Set single antena drving
    hw_int.write_register(RegisterA::io_configuration_1, std::byte { 0x80 });
    // Disable MCU CLK - not needed we don't drive MCU clk
    // TRY(change_register(RegisterA::IOConf1, std::byte { 0x07 }, std::byte { 0x07 }));

    // Enable MISO pulldowns
    hw_int.write_register(RegisterA::io_configuration_2, std::byte { 0x1C });

    // Set TX driver resistance
    set_output_amplitude(Amplitude::percent_82);
    sys_int.delay(1);

    hw_int.write_register(RegisterB::resistive_am_modulation, std::byte { 0x80 });
    hw_int.write_register(RegisterA::external_field_detector_activation_threshold, std::byte { 0x00 });
    hw_int.write_register(RegisterA::external_field_detector_deactivation_threshold, std::byte { 0x00 });

    hw_int.write_register(RegisterA::passive_target, std::byte { 0x50 });

    hw_int.write_register(RegisterA::passive_target_modulation, std::byte { 0x2F });

    hw_int.write_register(RegisterB::emd_suppression_configuration, std::byte { 0x40 });

    hw_int.write_register(RegisterB::auxiliary_modulation_setting, std::byte { 0x94 });

    hw_int.write_register(RegisterA::gain_reduction_state, std::byte { 0x09 });

    hw_int.write_register(RegisterA::antenna_tuning_control_1, std::byte { 0x82 });
    hw_int.write_register(RegisterA::antenna_tuning_control_2, std::byte { 0x82 });

    hw_int.write_register(RegisterA::regulator_voltage_control, std::byte { 0x00 });

    set_interrupt_mask(~IRQType::direct_command_finished);
    hw_int.direct_command(Command::adjust_regulators);

    {
        auto res = await_interrupt(IRQType::direct_command_finished, 500);
        if (!res.has_value()) {
            return res;
        }
    }

    set_interrupt_mask(IRQType::all);

    return {};
}

nfcv::Result<void> st25r39xxb::ST25R39XXB::turn_on_oscilator() {
    static constexpr std::byte OSCILATOR_ENABLE { 0b1000'0000 };
    if ((hw_int.read_register(RegisterA::operation_control) & OSCILATOR_ENABLE) != OSCILATOR_ENABLE) {
        set_interrupt_mask(~IRQType::oscilator_freq_stable);
        hw_int.change_register(RegisterA::operation_control, OSCILATOR_ENABLE, OSCILATOR_ENABLE);
        auto res = await_interrupt(IRQType::oscilator_freq_stable, 10);
        if (!res.has_value()) {
            return std::unexpected(res.error());
        }
    }

    set_interrupt_mask(IRQType::all);
    static constexpr std::byte OSCILATOR_OK { 0b0001'0000 };
    if ((hw_int.read_register(RegisterA::auxilary_display) & OSCILATOR_OK) != OSCILATOR_OK) {
        return std::unexpected(nfcv::Error::bad_oscilator);
    }

    return {};
}

void st25r39xxb::ST25R39XXB::nfcv_init_poller() {
    // Common NFC-V settings, for 26.48 kbps
    hw_int.write_register(RegisterA::receiver_configuration_1, std::byte { 0x13 });
    hw_int.write_register(RegisterA::receiver_configuration_2, std::byte { 0xed });
    hw_int.write_register(RegisterA::receiver_configuration_3, std::byte { 0x00 });
    hw_int.write_register(RegisterA::receiver_configuration_4, std::byte { 0x00 });

    hw_int.write_register(RegisterB::correlator_configuration_1, std::byte { 0x13 });
    hw_int.write_register(RegisterB::correlator_configuration_2, std::byte { 0x01 });

    // NFC-V poller settings
    hw_int.change_register(RegisterA::mode_definition, std::byte { 0x7c }, std::byte { 0x70 });
    hw_int.write_register(RegisterA::stream_mode_definition, std::byte { 0x38 });
    hw_int.register_clear_bits(RegisterB::auxiliary_modulation_setting, std::byte { 0x88 });
}

nfcv::Result<void> st25r39xxb::ST25R39XXB::nfcv_field_up() {
    // Martin Poupa's Solution - much simpler in flipper fw - will use that for the moment
    hw_int.write_register(RegisterB::nfc_field_on_guard_timer, std::byte { 0x00 });
    hw_int.write_register(RegisterA::operation_control, std::byte { 0x81 });
    hw_int.write_register(RegisterA::auxilary_definition, std::byte { 0x01 });
    set_interrupt_mask(~(IRQType::no_response_timer_expire | IRQType::minimum_guard_time_expires | IRQType::collision_detected | IRQType::pwp_field_active));
    hw_int.direct_command(Command::nfc_init_field_on);

    auto res = await_interrupt(IRQType::pwp_field_active, 100);
    if (!res.has_value()) {
        return std::unexpected(res.error());
    }

    set_interrupt_mask(~IRQType::no_response_timer_expire);
    hw_int.write_register(RegisterA::operation_control, std::byte { 0x8B });
    hw_int.write_register(RegisterA::operation_control, std::byte { 0xCB });

    hw_int.write_register(RegisterA::receiver_configuration_1, std::byte { 0x13 });
    hw_int.write_register(RegisterA::receiver_configuration_2, std::byte { 0xed });
    hw_int.write_register(RegisterA::receiver_configuration_3, std::byte { 0x00 });

    hw_int.write_register(RegisterB::correlator_configuration_1, std::byte { 0x13 });
    hw_int.write_register(RegisterB::correlator_configuration_2, std::byte { 0x01 });
    hw_int.write_register(RegisterA::stream_mode_definition, std::byte { 0x38 });
    hw_int.change_register(RegisterA::mode_definition, std::byte { 0x7c }, std::byte { 0x70 });

    hw_int.write_register(RegisterA::ISO14443A, std::byte { 0x00 });
    // Writes to undocumented registers
    // TODO: Validate that we don't need those (I am 100% sure that we don't, but there is no time for this)
    hw_int.write_register(static_cast<RegisterB>(0x34), std::byte { 0x01 }); // R 0x34 0x01
    hw_int.write_register(static_cast<RegisterB>(0x36), std::byte { 0x79 }); // R 0x36
    hw_int.write_register(static_cast<RegisterB>(0x37), std::byte { 0x07 }); // R 0x37 0x07

    hw_int.write_register(RegisterA::ISO14443A, std::byte { 0x1C });
    hw_int.write_register(static_cast<RegisterB>(0x1C), std::byte { 0 });

    sys_int.delay(6);

    hw_int.write_register(RegisterA::receiver_timer_mask, std::byte { 0x41 });

    hw_int.write_register(RegisterA::no_response_timer_1, std::byte { 0x00 });
    hw_int.write_register(RegisterA::no_response_timer_2, std::byte { 0x52 });

    hw_int.direct_command(Command::stop_all_1);
    hw_int.direct_command(Command::reset_rx_gain);

    hw_int.write_register(RegisterA::general_purpose_timer_1, std::byte { 0x01 });
    hw_int.write_register(RegisterA::general_purpose_timer_2, std::byte { 0x84 });
    hw_int.write_register(RegisterA::timer_and_emv_control, std::byte { 0x20 });

    hw_int.write_register(RegisterA::ISO14443A, std::byte { 0xDC });
    hw_int.write_register(RegisterA::receiver_configuration_2, std::byte { 0xE5 });

    set_interrupt_mask(IRQType::all);

    /*static constexpr std::byte OPER_CONTROL_TX_ENABLE { 0b0000'1000 };
    static constexpr std::byte OPER_CONTROL_RX_ENABLE { 0b0100'0000 };
    static constexpr std::byte OPER_CONTROL_EN_EXTERNAL_FIELD_DETECTOR_AUTOMATICALLY { 0b0000'0011 };
    if (!static_cast<bool>(TRY(read_register(RegisterA::OperContr)) & OPER_CONTROL_TX_ENABLE)) {
        TRY(write_register(RegisterB::NFCFieldOnGuardTimer, std::byte { 0x00 }));
        TRY(register_set_bits(RegisterA::OperContr, OPER_CONTROL_TX_ENABLE | OPER_CONTROL_RX_ENABLE | OPER_CONTROL_EN_EXTERNAL_FIELD_DETECTOR_AUTOMATICALLY));
    }*/

    return {};
}

void st25r39xxb::ST25R39XXB::nfcv_field_down() {
    static constexpr std::byte OPER_CONTROL_TX_ENABLE { 0b0000'1000 };
    static constexpr std::byte OPER_CONTROL_RX_ENABLE { 0b0100'0000 };
    hw_int.register_clear_bits(RegisterA::operation_control, OPER_CONTROL_TX_ENABLE | OPER_CONTROL_RX_ENABLE);
}

void st25r39xxb::ST25R39XXB::select_antenna(Antenna target_antenna) {
    static constexpr std::byte ANTENNA_CONTROL_MASK { 0b0100'0000 };
    std::byte value { 0 };
    if (target_antenna == Antenna::antenna_2) {
        value = ANTENNA_CONTROL_MASK;
    }
    hw_int.change_register(RegisterA::io_configuration_1, ANTENNA_CONTROL_MASK, value);

    set_output_amplitude(Amplitude::percent_82);
    set_output_impedance(Impedance::ohm2);
}

nfcv::Result<void> st25r39xxb::ST25R39XXB::nfcv_command(nfcv::Command &command) {
    // Prepare message
    buffer.clear();
    {
        const auto construct_res = nfcv::construct_command(buffer, command);
        if (!construct_res.has_value()) {
            return construct_res;
        }
    }

    set_interrupt_mask(~(IRQType::tx_end | IRQType::rx_start | IRQType::rx_end | IRQType::no_response_timer_expire | IRQType::errors));
    // Clear fifos
    hw_int.direct_command(Command::clear_fifo);

    // For write commands we need to wait longer for responses - according to iso from 10 to 20 ms.
    // FIXME: write the damn registers in 1 command
    if (nfcv::is_write_like_command(command)) {
        hw_int.write_register(RegisterA::no_response_timer_1, std::byte { 0x10 });
        hw_int.write_register(RegisterA::no_response_timer_2, std::byte { 0x8e });
    } else {
        hw_int.write_register(RegisterA::no_response_timer_1, std::byte { 0x00 });
        hw_int.write_register(RegisterA::no_response_timer_2, std::byte { 0x52 });
    }

    // Pass tx_buffer size to the chip (the length is calculated in bits, that why * 8)
    // The size is technically in bits, but only NFC-A supports writing in bits
    uint16_t tx_buffer_len = buffer.size() * 8;
    // Because how the registers are ordererd we need to convert the number to "big endian"
    tx_buffer_len = std::byteswap(tx_buffer_len);
    hw_int.write_registers_continuous(RegisterA::number_of_transmitted_bytes_1, std::span { reinterpret_cast<std::byte *>(&tx_buffer_len), sizeof(tx_buffer_len) });

    // Write the data to the fifo
    hw_int.write_fifo(buffer);

    // Start transmission
    hw_int.direct_command(Command::transmit_without_crc); // Temporary without CRC until I make it calculate it
    auto res = await_interrupt(IRQType::tx_end, 10);
    if (!res.has_value()) {
        return res;
    }
    // Transmission finished

    // Some commands doesn't send a response according to the iso - like StayQuiet
    if (!nfcv::is_response_expected(command)) {
        sys_int.delay(2); // FIXME: Add proper timeout using the generict timer in st25r39xxb chips
        return {};
    }
    // TODO: Use no response timer here instead
    const auto receive_res = await_interrupt(IRQType::rx_start, 21);
    if (!receive_res.has_value()) {
        // We timed out - we didn't receive any response
        if (receive_res.error() == nfcv::Error::timeout) {
            return std::unexpected(nfcv::Error::no_response);
        }
        return res;
    }
    res = await_interrupt(IRQType::rx_end, 20);
    if (!res.has_value()) {
        return res;
    }

    // Read received data
    buffer.clear();
    buffer.resize(get_fifo_len());
    if (!buffer.empty()) {
        hw_int.read_fifo(buffer);
    }

    const auto decode_res = nfcv::decode(buffer, buffer);
    if (!decode_res.has_value()) {
        return std::unexpected(decode_res.error());
    }
    if (decode_res->size() < 3) {
        return std::unexpected(nfcv::Error::response_invalid_size);
    }

    /*{
        iso13239::CRC crc {};
        crc.add_bytes(std::span { reinterpret_cast<uint8_t *>(buffer.data()), *decode_res - 2 });
        const auto transmitted_crc = std::to_integer<uint16_t>(buffer[buffer.size() - 2]) | (std::to_integer<uint16_t>(buffer[buffer.size() - 1]) << 8);
        if (crc.get_result() != transmitted_crc) {
            return std::unexpected(Error::InvalidCRC);
        }
    }*/

    // Parse response to message
    const auto deserialization_res = nfcv::parse_response(*decode_res, command);
    if (!deserialization_res.has_value()) {
        return std::unexpected(deserialization_res.error());
    }
    return {};
}

uint16_t st25r39xxb::ST25R39XXB::get_fifo_len() {
    std::array<std::byte, 2> fifo_status {};
    hw_int.read_registers_continuous(st25r39xxb::RegisterA::fifo_status_1, fifo_status);
    return (std::to_integer<uint16_t>(fifo_status.at(0)) | ((std::to_integer<uint16_t>(fifo_status.at(1)) & 0xB0) << 2));
}

nfcv::Result<void> st25r39xxb::ST25R39XXB::nfcv_tick_poller() {
    using namespace nfcv;
    // WARNING: This function is just validation for the current implementation. It shouldn't be used in production
    //
    // Let's discuss what it does:
    // * switches antenna (so we use both - but we just one between calls)
    // * powers on a field
    //   * waits 50 ms for it to be stable and for it to charge up any tags in vicinity
    // * sends Inventory command to locate any tags
    // * if none found it exists
    // * sends Stay Quiet to any found tag to prevent it to accept broadcasts
    // * validate received data
    // * read first block
    // * if first block is 0xDEADBEEF, then we do nothing and return early
    // * otherwise we fill the tag with 0xDEADBEEF
    switch_antennas();
    auto res = nfcv_field_up();
    if (!res.has_value()) {
        return std::unexpected(res.error());
    }
    const auto auto_field_down = ScopeGuard { [&]() {
        nfcv_field_down();
    } };
    sys_int.delay(50);

    nfcv::UID uid;
    nfcv::Command cmd = command::Inventory {
        .request = {},
        .response = {
            .uid = uid,
        }
    };
    auto cmd_res = nfcv_command(cmd);
    if (!cmd_res.has_value()) {
        if (cmd_res.error() != Error::no_response) {
            return std::unexpected(cmd_res.error());
        }
        return {};
    }
    sys_int.delay(2);

    cmd = command::StayQuiet {
        .request = { .uid = uid },
    };
    cmd_res = nfcv_command(cmd);
    if (!cmd_res.has_value()) {
        return std::unexpected(cmd_res.error());
    }
    sys_int.delay(2);

    cmd = command::SystemInfo {
        .request = { .uid = uid },
        .response = {},
    };
    cmd_res = nfcv_command(cmd);
    if (!cmd_res.has_value()) {
        return std::unexpected(cmd_res.error());
    }
    const auto tag_number_of_blocks = std::get<command::SystemInfo>(cmd).response.tag_size.value().number_of_blocks;

    static constexpr std::array<std::byte, 4> DEAD_BEEF = { std::byte { 0xde }, std::byte { 0xad }, std::byte { 0xbe }, std::byte { 0xef } };
    {
        sys_int.delay(2);
        std::array<std::byte, 4> block_buffer {};

        cmd = command::ReadSingleBlock {
            .request = { .uid = uid, .block_address = 0 },
            .response = { .block_buffer = block_buffer },
        };
        cmd_res = nfcv_command(cmd);
        if (!cmd_res.has_value()) {
            return std::unexpected(cmd_res.error());
        }

        if (block_buffer == DEAD_BEEF) {
            return {};
        }
    }

    {
        sys_int.delay(2);
        std::array<std::byte, 4> block_buffer = DEAD_BEEF;

        for (uint8_t i = 0; i < tag_number_of_blocks; ++i) {
            cmd = command::WriteSingleBlock {
                .request = { .uid = uid, .block_address = 0, .block_buffer = block_buffer },
                .response = {},
            };
            cmd_res = nfcv_command(cmd);
            if (!cmd_res.has_value()) {
                return std::unexpected(cmd_res.error());
            }
            sys_int.delay(2);
        }
    }
    return {};
}

void st25r39xxb::ST25R39XXB::switch_antennas() {
    switch (current_antenna) {
    case Antenna::antenna_1:
        current_antenna = Antenna::antenna_2;
        break;
    case Antenna::antenna_2:
        current_antenna = Antenna::antenna_1;
        break;
    }
    select_antenna(current_antenna);
}

void st25r39xxb::ST25R39XXB::set_output_impedance(st25r39xxb::Impedance target_impedance) {
    using namespace st25r39xxb;
    hw_int.change_register(RegisterA::tx_driver, std::byte { 0x0f }, std::byte { std::to_underlying(target_impedance) });
}

void st25r39xxb::ST25R39XXB::set_output_amplitude(st25r39xxb::Amplitude target_amplitude) {
    using namespace st25r39xxb;
    hw_int.change_register(RegisterA::tx_driver, std::byte { 0xf0 }, std::byte { std::to_underlying(target_amplitude) });
}
