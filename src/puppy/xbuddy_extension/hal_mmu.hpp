/// @file
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace hal::mmu {

/// Initialize MMU UART and pins.
void init();

/// MSP Initialization for MMU UART, for internal use only.
void msp_init(void *);

/// RX callback, for internal use only.
void rx_callback(void *, uint16_t size);

/// TX callback, for internal use only.
void tx_callback(void *);

/// Transmit bytes on MMU UART.
/// Blocks until all bytes are transmitted.
void transmit(std::span<const std::byte>);

/// Receive bytes from MMU UART.
/// Bytes are received into supplied buffer.
/// Returns view into that buffer.
/// Does not block.
std::span<std::byte> receive(std::span<std::byte>);

/// Flush the receive buffer, discarding its contents.
void flush();

/// Control the power pin of the MMU.
void power_pin_set(bool);

/// Control the nreset pin of the MMU.
void nreset_pin_set(bool);

/// Read the power pin of the MMU.
bool power_pin_get();

/// Read the nreset pin of the MMU.
bool nreset_pin_get();

} // namespace hal::mmu
