#include <sys/reent.h>
#include <sys/stat.h>

#include "hal.h"

extern "C" {

#ifdef STM32H5
int _close_r(struct _reent *, int) {
    hal_panic();
}

_off_t _lseek_r(struct _reent *, int, _off_t, int) {
    hal_panic();
}

_ssize_t _read_r(struct _reent *, int, void *, size_t) {
    hal_panic();
}

_ssize_t _write_r(struct _reent *, int, const void *, size_t) {
    return 0;
}

int __attribute__((used)) _kill_r(struct _reent *, int, int) {
    hal_panic();
}

int __attribute__((used)) _getpid_r(struct _reent *) {
    return -1;
}

int _fstat(int, struct stat *) {
    hal_panic();
}

int _isatty(int) {
    hal_panic();
}

#elifdef STM32C0
int _close(struct _reent *, int) {
    hal_panic();
}

_off_t _lseek(struct _reent *, int, _off_t, int) {
    hal_panic();
}

_ssize_t _read(struct _reent *, int, void *, size_t) {
    hal_panic();
}

_ssize_t _write(struct _reent *, int, const void *, size_t) {
    hal_panic();
}
#endif
}
