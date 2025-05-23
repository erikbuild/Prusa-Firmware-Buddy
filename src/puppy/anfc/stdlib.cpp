#include <sys/reent.h>
#include <sys/stat.h>
#include <errno.h>
#include <freertos/critical_section.hpp>
#include <malloc.h>
#include <FreeRTOS.h>

#include "hal.h"

// Reserve some bytes for the base stack (used for ISR, tasks have their own stacks statically allocated)
#define ISR_STACK_LENGTH_BYTES 256

extern "C" {
extern char heap_start asm("end");
extern char heap_limit asm("_estack");
char *heap_end = &heap_start;

[[noreturn]] void _posioned();

// Overriding a weak symbol here
void *sbrk(int) {
    // Shouldn't get called, _sbrk_r should be the only function
    _posioned();
}

// Overriding a weak symbol here
void *_sbrk(int) {
    // Shouldn't get called, _sbrk_r should be the only function
    _posioned();
};

void *_sbrk_r(struct _reent *, int incr) {
    volatile const char *prev_heap_end = heap_end;

    {
        freertos::CriticalSection cs;
        if (heap_end + incr > &heap_limit - ISR_STACK_LENGTH_BYTES) {
            errno = ENOMEM;
            return caddr_t(-1);
        }

        heap_end += incr;
    }

    return caddr_t(prev_heap_end);
}

// Malloc is not thread-safe by default, we gotta override these symbols and introduce a lock
void __malloc_lock(struct _reent *) {
    portENTER_CRITICAL();
}

void __malloc_unlock(struct _reent *) {
    portEXIT_CRITICAL();
}

extern "C" void __assert_func(const char *, int, const char *, const char *) {
    hal_panic();
}

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
