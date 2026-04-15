/// @file
#pragma once

#include <cstdint>
#include <optional>

namespace hal::spi {

void spi1_init();
void dma_isr();
void init_comm();

namespace accel {
    struct Sample {
        int16_t x;
        int16_t y;
        int16_t z;
    };
    /// Retrieves single sample if available
    ///
    /// The sample must be available so call this after you receive data ready interupt
    /// Thread unsafe - call only from spi task
    /// Returns false if SPI fails or sample overrun
    [[nodiscard]] bool get_sample(Sample &);

} // namespace accel

namespace loadcell {
    /// Thread unsafe - call only from spi task
    void init();

    /// The sample must be available so call this after you receive data ready interupt
    /// Thread unsafe - call only from spi task
    std::optional<uint32_t> get_sample();
} // namespace loadcell

} // namespace hal::spi
