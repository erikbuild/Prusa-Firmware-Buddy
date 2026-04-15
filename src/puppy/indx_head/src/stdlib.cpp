#include <sys/reent.h>
#include <sys/stat.h>

#include <cstdlib>

extern "C" {

void __assert_func(const char *, int, const char *, const char *) {
    std::abort();
}

int _close(struct _reent *, int) {
    std::abort();
}

_off_t _lseek(struct _reent *, int, _off_t, int) {
    std::abort();
}

_ssize_t _read(struct _reent *, int, void *, size_t) {
    std::abort();
}

_ssize_t _write(struct _reent *, int, const void *, size_t) {
    std::abort();
}

int __attribute__((used)) _getpid() {
    return -1;
}

int __attribute__((used)) _kill(int, int) {
    std::abort();
}
}
