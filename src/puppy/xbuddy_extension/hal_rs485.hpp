/// @file
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace hal::rs485 {

/// Initialize RS485 UART.
void init();

/// MSP Initialization for RS485 UART, for internal use only.
void msp_init(void *);

/// RX callback, for internal use only.
void rx_callback(void *, uint16_t size);

/// TX callback, for internal use only.
void tx_callback(void *);

/// Error callback, for internal use only.
void error_callback(void *);

/// Start receiving messages.
/// Does not block.
void start_receiving();

/// Blocks until message is received.
/// Returned span is valid until next transmit()
std::span<std::byte> receive();

/// Blocks until message is received or timeout occurs.
/// Returned span is valid until next transmit()
/// Returns empty span if timeout occurs.
std::span<std::byte> receive_timeout(uint32_t timeout_ms);

/// Transmit message.
/// Does not block.
/// Supplied span must remain valid until next receive()
void transmit_and_then_start_receiving(std::span<std::byte>);

/// Clear bus errors if needed.
void housekeeping();

} // namespace hal::rs485
