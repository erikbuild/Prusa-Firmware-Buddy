#include <selftest/selftest_invocation.hpp>

#include <atomic>

namespace selftest_invocation {

namespace {
    std::atomic<bool> aborted_ = false;
}

void begin() {
    aborted_ = false;
}

void mark_aborted() {
    aborted_ = true;
}

bool is_aborted() {
    return aborted_;
}

} // namespace selftest_invocation
