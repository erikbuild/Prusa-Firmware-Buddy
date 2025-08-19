#include <catch2/catch.hpp>
#include <bsod/bsod.h>

extern "C" void _bsod(const char *fmt, const char *file_name, int line_number, ...) {
    FAIL(fmt);
}
