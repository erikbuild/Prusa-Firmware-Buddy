#include "can_driver_uart.hpp"

#include <cobs/cobs.hpp>
#include <cstring>
#include <array>
#include <timing.h>
#include <crc/crc.hpp>

namespace can {
static UartDriver *instance = nullptr;

UartDriver::UartDriver(UART_HandleTypeDef &huart)
    : huart(huart) {
    if (instance != nullptr) {
        bsod("UART supports only one driver");
    }
    instance = this;
}

UartDriver &UartDriver::get_driver([[maybe_unused]] UART_HandleTypeDef *huart_isr) {
    UartDriver *driver = instance;
    assert(driver != nullptr && &driver->huart == huart_isr);
    return *driver;
}

bool UartDriver::send(const CanardFrame &frame, bool store_timestamp) {
    UNUSED(store_timestamp);
    // Check maximal length
    assert(frame.payload_size <= uart::MAX_PAYLOAD_SIZE); // 64 is max size of CAN FD payload, 2 is the start & stop byte

    // Check if tx process is not already ongoing (before send buffer is overwritten)
    if (this->huart.gState != HAL_UART_STATE_READY) {
        error_stats.tec++;
        error_stats.err_log++;
        return false;
    }
    // Prepare the raw payload for COBS encoding and CRC calculation.
    // - Format: [CAN ID (4B)] [payload length (1B)] [payload (0-64B)] [CRC16 (2B)]
    std::array<uint8_t, uart::MAX_RAW_FRAME_SIZE> raw_packet_data;

    // Pack the 29-bit CAN ID (assuming little-endian byte order)
    static_assert(std::endian::native == std::endian::little);
    memcpy(&raw_packet_data[0], &frame.extended_can_id, sizeof(frame.extended_can_id));
    // pack payload size
    raw_packet_data[4] = static_cast<uint8_t>(frame.payload_size);
    assert(frame.payload != nullptr);
    // pack payload
    memcpy(&raw_packet_data[5], frame.payload, frame.payload_size);

    // Calculate CRC
    uint32_t crc_data_len = uart::HEADER_SIZE + frame.payload_size;
    uint16_t crc = Crc16CcittFalse().update(raw_packet_data.data(), crc_data_len).get();
    memcpy(&raw_packet_data[crc_data_len], &crc, uart::CRC_SIZE);
    size_t packet_len = crc_data_len + uart::CRC_SIZE; // Add 2 bytes of CRC16

    auto encoded_packet_len = cobs::encode(raw_packet_data.data(), packet_len, send_buffer.data(), send_buffer.size());
    if (!encoded_packet_len.has_value()) {
        bsod_unreachable();
    };

    send_buffer[encoded_packet_len.value()] = 0x00; // delimeter
    size_t total_len = encoded_packet_len.value() + 1;

    if (HAL_UART_Transmit_IT(&this->huart, send_buffer.data(), total_len) != HAL_OK) {
        error_stats.tec++;
        error_stats.err_log++;
        return false;
    }

    return true;
}

void UartDriver::start_listening() {
    HAL_StatusTypeDef status = HAL_UART_Receive_IT(&huart, &uart_rx_byte, 1);

    if (status != HAL_OK) {
        bsod("UART start listening failed");
    }
}

bool UartDriver::receive(CanardFrame &frame, CanardMicrosecond *timestamp_us) {

    ReceiveBuffer &recv_buffer = receive_buffers[receive_buffer_i];
    if (!recv_buffer.contains_message) {
        return false;
    }

    std::array<uint8_t, uart::MAX_RAW_FRAME_SIZE> decoded_data;
    auto decoded_size = cobs::decode(recv_buffer.data.data(), recv_buffer.used_size, decoded_data.data(), decoded_data.size());
    if (!decoded_size.has_value()) {
        bsod_unreachable();
    }
    receive_buffer_i = (receive_buffer_i + 1) % receive_buffers.size();
    recv_buffer.reset();

    // TODO: RX timestamp should be handled in HAL_UART_RxCpltCallback if it is expected to be more accurate
    *timestamp_us = get_timestamp_us();
    // extract message
    memcpy(&frame.extended_can_id, &decoded_data[0], sizeof(frame.extended_can_id));
    frame.payload_size = decoded_data[4];
    if (decoded_size.value() != static_cast<size_t>(uart::HEADER_SIZE + frame.payload_size + uart::CRC_SIZE)) {
        assert(false);
        return false;
    }
    memcpy(payload_buffer, &decoded_data[5], frame.payload_size);
    frame.payload = payload_buffer;

    // Extract received CRC (little-endian)
    const size_t crc_pos = uart::HEADER_SIZE + frame.payload_size;
    uint16_t received_crc;
    memcpy(&received_crc, &decoded_data[crc_pos], uart::CRC_SIZE);

    // Calculate and check CRC of the extracted message (excluding the CRC bytes)
    const uint16_t calculated_crc = Crc16CcittFalse().update(decoded_data.data(), uart::HEADER_SIZE + frame.payload_size).get();
    if (received_crc != calculated_crc) {
        assert(false);
        return false;
    }

    return true;
}

void UartDriver::rx_callback() {
    UartDriver::ReceiveBuffer &recv_buffer = receive_buffers[rx_callback_i];

    // Check if current receive buffer has a unprocessed message
    if (recv_buffer.contains_message) {
        isr_notify(Driver::Notification::RxLost);
    }

    // end of a non-empty message
    else if (uart_rx_byte == '\0') {
        // only accept incoming '\0' bytes if they signal end of a message
        if (recv_buffer.used_size != 0) {
            recv_buffer.contains_message = true;
            rx_callback_i = (rx_callback_i + 1) % receive_buffers.size();
            isr_notify(Driver::Notification::RxDone);
        }
    }
    // byte of a message
    else {
        recv_buffer.put(uart_rx_byte);
    }
    start_listening();
}

void UartDriver::tx_callback() {
    isr_notify(Driver::Notification::TxDone);
    tx_timestamp.set(get_timestamp_us());
}

void UartDriver::error_callback() {
    // Note: The 'tec' counter is not handled here. In an interrupt-driven UART
    // implementation, transmit errors would typically be managed in the send()
    // method itself (e.g., if HAL_UART_Transmit_IT() returns a failure status)
    // and not via this global callback.
    error_stats.rec++;
    error_stats.err_log++;
}

extern "C" {

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    auto &driver = UartDriver::get_driver(huart);
    driver.rx_callback();
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
    auto &driver = UartDriver::get_driver(huart);
    driver.tx_callback();
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
    auto &driver = UartDriver::get_driver(huart);
    driver.error_callback();
}
}

} // namespace can
