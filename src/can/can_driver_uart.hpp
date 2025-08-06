#pragma once

#include <can_driver.hpp>
#include <device/hal.h>

#include <atomic>
#include <optional>
#include <utils/atomic_circular_queue.hpp>

namespace can {

/*
 * @brief This driver adapts the OpenCyphal/CAN protocol to run over UART.
 *
 * It encapsulates CAN frames within a custom, COBS-encoded message format.
 * The full message transmitted over UART is:
 *
 * [ COBS-Encoded Data ] [ End-of-Packet Delimiter (0x00) ]
 *
 * The raw CAN frame data within the COBS packet has this structure:
 * [ CAN ID (4B) | Payload Length (1B) | Payload (0-64B) | CRC16 (2B) ]
 *
 * Provided structure of UART messages
 * ┌────────────────────────────────┐
 * │          [Stuffed Data]        │ (Variable length,)
 * │Contains:                       │
 * │   - Header  (5B)               │  Header: CAN ID (4B) + payload length (1B) = 5B
 * │   - payload (0-64B)            │
 * │   - CRC16   (2B)               │
 * ├────────────────────────────────┤
 * │           [Stop Byte]          │ (1 byte, 0x00)
 * └────────────────────────────────┘
 *
 */

namespace uart {
    const uint8_t DELIMETER_SIZE = 1U; // (0x00)
    static constexpr uint8_t HEADER_SIZE = 4U + 1U; // CAN ID (4) + payload length (1)
    static constexpr uint8_t MAX_PAYLOAD_SIZE = CANARD_MTU_CAN_FD; // 64 bytes for CAN FD
    static constexpr uint8_t CRC_SIZE = 2U; // CRC16
    static constexpr uint8_t MAX_RAW_FRAME_SIZE = HEADER_SIZE + MAX_PAYLOAD_SIZE + CRC_SIZE; // 5 + 64 + 2 = 71 bytes
    // COBS encoding adds a small overhead and a delimiter byte.
    static constexpr size_t MAX_ENCODED_FRAME_SIZE = MAX_RAW_FRAME_SIZE + ((MAX_RAW_FRAME_SIZE + 254) / 254) + DELIMETER_SIZE;
} // namespace uart

/**
 * @brief Acts as an driver+adapter of the OpenCyphal/CAN protocol for use over a UART interface.
 *
 * This class allows the libcanard library to operate on a UART connection
 * by converting CAN frames into a custom, COBS-encoded message structure
 * for transmission, and deserializing them upon reception.
 */
class UartDriver : public Driver {

    /*
    Currently used as a UART version of thread safe CanardMicrosecond
    -   if a approach in atomic_c0.cpp would be ported to H5 and appropriate atomic.h operations were stubbed
        this class could be replaced by a simple std::atomic<CanardMicrosecond>
    */
    struct UartAtomicTimestamp {
        uint64_t timestamp = 0; // value in microseconds

        void set(uint64_t value) {
            __disable_irq();
            timestamp = value;
            __enable_irq();
        }

        uint64_t get() {
            uint64_t temp;
            __disable_irq(); // Enter critical section
            temp = timestamp;
            __enable_irq(); // Exit critical section
            return temp;
        }
    };
    struct ReceiveBuffer {
        std::array<uint8_t, uart::MAX_ENCODED_FRAME_SIZE> data;
        size_t used_size = 0;
        std::atomic<bool> contains_message = false;

        void put(uint8_t val) {
            if (used_size >= data.size()) {
                bsod_unreachable();
            }
            data[used_size++] = val;
        }

        void reset() {
            contains_message = false;
            used_size = 0;
        }
    };

    UartAtomicTimestamp tx_timestamp; // timestamp of the last successful transmission

    std::array<uint8_t, uart::MAX_ENCODED_FRAME_SIZE> send_buffer; // buffer for one frame to be transmitted
    std::array<ReceiveBuffer, 2> receive_buffers;
    uint8_t receive_buffer_i = 0, rx_callback_i = 0;
    uint8_t payload_buffer[CANARD_MTU_MAX] = { 0 };

    ErrorStats error_stats = ErrorStats {
        .tec = 0,
        .rec = 0,
        .err_log = 0,
    };

    uint8_t uart_rx_byte;
    UART_HandleTypeDef &huart; // HAL UART Instance

public:
    UartDriver(UART_HandleTypeDef &huart);
    void start_listening();
    bool send(const CanardFrame &frame, bool store_timestamp = false) override;
    bool receive(CanardFrame &frame, CanardMicrosecond *timestamp_us = nullptr) override;
    void start(bool automatic_retransmission_enable = true) override {
        UNUSED(automatic_retransmission_enable);
        start_listening();
    }
    void set_automatic_retransmission(bool enable) override { UNUSED(enable); }
    std::optional<CanardMicrosecond> get_sent_timestamp() override { return (tx_timestamp.get() != 0) ? std::optional<CanardMicrosecond> { tx_timestamp.get() } : std::nullopt; }

    // Filter dummies
    uint32_t filter_count() const override { return 0; };
    void set_filter(uint32_t index, const CanardFilter &filter, bool timestamp, bool high_prio) override {
        UNUSED(index), UNUSED(filter), UNUSED(timestamp), UNUSED(high_prio);
    };

    // Intentionally not virtual
    // Vitrual destructors generate a free() call, not acceptable for no dynamic allocation targets
    ~UartDriver() = default;

    /**
     * @brief Get error statistics.
     * @note Reading clears err_log counter.
     * @return error statistics
     */
    ErrorStats get_error_stats() override {
        ErrorStats temp = error_stats;
        error_stats = ErrorStats {
            .tec = 0,
            .rec = 0,
            .err_log = 0,
        };

        return temp;
    };

    // -- Interrupt Callbacks --
    // These are public but are not intended for direct use.
    // They act as a bridge from the HAL's C-style interrupts.

    /**
     * @brief Get driver that uses this HAL instance.
     * @param hfdcan_isr HAL instance
     * @return driver
     */
    static UartDriver &get_driver(UART_HandleTypeDef *huart_isr);

    /**
     * @brief RX completed callback HAL_UART_RxCpltCallback().
     */
    void rx_callback();

    /**
     * @brief TX completed callback HAL_UART_TxCpltCallback().
     */
    void tx_callback();

    /**
     * @brief Receive error callback HAL_UART_ErrorCallback().
     */
    void error_callback();
};
} // namespace can
