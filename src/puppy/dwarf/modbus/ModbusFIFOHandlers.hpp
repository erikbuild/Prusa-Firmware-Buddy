#pragma once

#include <cstddef>
#include <cstdint>
#include <array>

#include <fifo_coder/fifo_coder.hpp>

using namespace fifo_coder;

/// FIFO handler for encoded data stream
size_t handle_encoded_fifo(std::array<uint16_t, MODBUS_FIFO_LEN> &fifo);
