#include <st25r39xxb/hw_interface.hpp>

#include <algorithm>
#include <cassert>

namespace st25r39xxb {

namespace {
    constexpr std::byte REG_B_MARK { 0xFB };
}

void SpiInterface::transmit(const std::span<const std::byte> &tx) {
    chip_select();
    unsafe_transmit(tx);
    chip_deselect();
}

void SpiInterface::transmit_receive(const std::span<const std::byte> &tx, const std::span<std::byte> &rx) {
    chip_select();
    unsafe_transmit(tx);
    unsafe_receive(rx);
    chip_deselect();
}

void SpiInterface::receive(const std::span<std::byte> &rx) {
    chip_select();
    unsafe_receive(rx);
    chip_deselect();
}

void SpiInterface::read_registers_continuous(st25r39xxb::RegisterA reg, const std::span<std::byte> &data) {
    using namespace st25r39xxb;
    std::array<std::byte, 1> read_command { std::byte { std::to_underlying(reg) } | std::byte { 1 << 6 } };
    transmit_receive(read_command, data);
}

void SpiInterface::read_registers_continuous(st25r39xxb::RegisterB reg, const std::span<std::byte> &data) {
    std::array<std::byte, 2> command = { REG_B_MARK, std::byte { std::to_underlying(reg) } | std::byte { 1 << 6 } };
    transmit_receive(command, data);
}

void SpiInterface::write_registers_continuous(st25r39xxb::RegisterA reg, const std::span<const std::byte> &data) {
    using namespace st25r39xxb;
    assert(data.size() <= buffer.size() - 1);
    buffer[0] = static_cast<std::byte>(reg);
    std::copy_n(data.begin(), data.size(), std::next(buffer.begin()));
    transmit(std::span { buffer.data(), data.size() + 1 });
}

void SpiInterface::write_registers_continuous(st25r39xxb::RegisterB reg, const std::span<const std::byte> &data) {
    using namespace st25r39xxb;
    assert(data.size() <= buffer.size() - 2);
    buffer[0] = REG_B_MARK;
    buffer[1] = static_cast<std::byte>(reg);
    std::copy_n(data.begin(), data.size(), std::next(buffer.begin(), 2));
    transmit(std::span { buffer.data(), data.size() + 2 });
}

void SpiInterface::read_fifo(const std::span<std::byte> &data) {
    assert(data.size() <= constant::FIFO_SIZE);
    static constexpr std::array<std::byte, 1> read_fifo { std::byte { 0x9F } };
    chip_select();
    unsafe_transmit(std::span { read_fifo });
    unsafe_receive(data);
    chip_deselect();
}

void SpiInterface::write_fifo(const std::span<const std::byte> &data) {
    assert(data.size() <= constant::FIFO_SIZE);
    static constexpr std::array<std::byte, 1> write_fifo { std::byte { 0x80 } };
    chip_select();
    unsafe_transmit(std::span { write_fifo });
    unsafe_transmit(data);
    chip_deselect();
}

void SpiInterface::direct_command(st25r39xxb::Command command) {
    // NOTE: The | is redundant here, since all the enum values are already encoded this way, but I keep just to be sure
    std::array<std::byte, 1> buffer = { std::byte { std::to_underlying(command) } | std::byte { 3 << 6 } };
    transmit(buffer);
}

} // namespace st25r39xxb
