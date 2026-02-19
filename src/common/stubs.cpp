#include "bsod.h"
#include "safe_state.h"
#include <common/sys.hpp>

void abort() {
    bsod("aborted");
}

void __assert_func(const char *file, int line, const char * /*func*/, const char *msg) {
    _bsod("ASSERT %s", file, line, msg);
}

extern "C" int _isatty(int __attribute__((unused)) fd) {
    // TTYs are not supported
    return 0;
}
