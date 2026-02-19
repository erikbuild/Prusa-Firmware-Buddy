#include "bsod.h"
#include "safe_state.h"
#include <common/sys.hpp>

void abort() {
    bsod("aborted");
}

void __assert_func(const char *file_path, int line, const char * /*func*/, const char *msg) {
    const char *slash_idx = std::strrchr(file_path, '/');
    const char *file_name = slash_idx != nullptr ? slash_idx + 1 : file_path;
    _bsod("ASSERT %s", file_name, line, msg);
}

extern "C" int _isatty(int __attribute__((unused)) fd) {
    // TTYs are not supported
    return 0;
}
