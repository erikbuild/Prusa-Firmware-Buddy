/// @file
#pragma once

namespace bootloader_update {

/// Compares the bootloader image embedded into this firmware with the actual
/// bootloader. On mismatch, reprograms the bootloader. This will brick the
/// board if power supply fails or the board is reset during the programming
/// of the first sector. Risk was assessed and benefits were deemed worthy.
void run();

} // namespace bootloader_update
