#include <nfcv/commands.hpp>

namespace nfcv {
namespace {
    template <typename Command>
    constexpr bool is_write_like_command([[maybe_unused]] const Command &) {
        return false;
    }

    template <>
    constexpr bool is_write_like_command<command::WriteSingleBlock>([[maybe_unused]] const command::WriteSingleBlock &) {
        return true;
    }
} // namespace

bool is_response_expected(const Command &command) {
    return std::visit([]<typename T>(const T &) -> bool { return requires { typename T::Response; }; }, command);
}

bool is_write_like_command(const Command &command) {
    return std::visit([](const auto &cmd) { return is_write_like_command(cmd); }, command);
}

} // namespace nfcv
