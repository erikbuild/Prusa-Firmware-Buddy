/// @file
#include "bootloader_update.hpp"

#include <stm32c0xx.h>
#include <cstdint>
#include <cstring>
#include <raii/scope_guard.hpp>
#include "rtt.hpp"
#include "hal.hpp"
#include "hal_watchdog.hpp"

/// Feel free to redefine to `true` when developing just the FLASH driver
/// on `release` builds.
#define FLASH_DEBUG() !defined(NDEBUG)

#if FLASH_DEBUG()
    /// This is NOT error checking! These assertions are only used to ensure
    /// invariants when developing FLASH driver and to help documenting them.
    /// Note: can't hal::panic() because hal::init() was not called yet.
    #define FLASH_ASSERT(reason, assertion) \
        if (assertion) [[likely]] {         \
        } else {                            \
            rtt::print(reason);             \
        }

    #warning "FLASH_DEBUG() must be disabled in production build"

#else
    #define FLASH_ASSERT(...)
#endif

#if BOOTLOADER_SIZE > 0

static_assert(BOOTLOADER_SIZE % FLASH_PAGE_SIZE == 0, "BOOTLOADER_SIZE must be a whole number of flash pages");
static_assert(BOOTLOADER_SIZE % 8 == 0, "BOOTLOADER_SIZE must be a whole number of 64-bit flash words");

extern "C" const uint32_t bootloader_code[];

namespace {

uint32_t *const bootloader_region_start = (uint32_t *)FLASH_BASE;

[[nodiscard]] bool flash_wait_for_last_operation() {
    while (READ_BIT(FLASH->SR, FLASH_SR_BSY1)) {
    }
    const uint32_t error = FLASH->SR & FLASH_FLAG_SR_ERROR;
    WRITE_REG(FLASH->SR, error);
    while (READ_BIT(FLASH->SR, FLASH_SR_CFGBSY)) {
    }
    if (error) {
        rtt::print("fail sr=", error, "\n");
    }
    return error == 0;
}

void run_internal() {
    FLASH_ASSERT("Flash must be locked before calling this\n", READ_BIT(FLASH->CR, FLASH_CR_LOCK));
    WRITE_REG(FLASH->KEYR, FLASH_KEY1);
    WRITE_REG(FLASH->KEYR, FLASH_KEY2);
    FLASH_ASSERT("Flash must have been unlocked now\n", !READ_BIT(FLASH->CR, FLASH_CR_LOCK));

    // Restore FLASH->CR to a clean, locked state on every exit path so a
    // failed attempt does not poison the next retry.
    ScopeGuard cleanup = [] {
        CLEAR_BIT(FLASH->CR, FLASH_CR_PER | FLASH_CR_PG | FLASH_CR_PNB);
        WRITE_REG(FLASH->SR, FLASH_FLAG_SR_ERROR);
        SET_BIT(FLASH->CR, FLASH_CR_LOCK);
    };

    if (!flash_wait_for_last_operation()) {
        return;
    }

    constexpr uint32_t num_pages = BOOTLOADER_SIZE / FLASH_PAGE_SIZE;
    constexpr uint32_t words_per_page = FLASH_PAGE_SIZE / sizeof(uint32_t);
    constexpr uint32_t doublewords_per_page = FLASH_PAGE_SIZE / 8;
    static_assert(FLASH_PAGE_SIZE % 8 == 0, "FLASH_PAGE_SIZE must be a whole number of 64-bit flash words");

    for (uint32_t page = 0; page < num_pages; page++) {
        const uint32_t *const src_page = &bootloader_code[page * words_per_page];
        uint32_t *const dest_page = &bootloader_region_start[page * words_per_page];

        if (std::memcmp(dest_page, src_page, FLASH_PAGE_SIZE) == 0) {
            // Do not flash what was already flashed. This reduces likelihood
            // of bricking the board, because first page of the bootloader
            // contains small "preboot" program. Preboot checks integrity
            // of the rest of the bootloader and jumps to application if
            // the bootloader is corrupted. This means almost all errors should
            // eventually self-heal.
            continue;
        }
        hal::watchdog::kick();

        // Erase the page
        const uint32_t cr_no_pnb = FLASH->CR & ~FLASH_CR_PNB;
        FLASH->CR = cr_no_pnb | (FLASH_CR_STRT | (page << FLASH_CR_PNB_Pos) | FLASH_CR_PER);
        if (!flash_wait_for_last_operation()) {
            return;
        }
        CLEAR_BIT(FLASH->CR, FLASH_CR_PER | FLASH_CR_PNB);
        hal::watchdog::kick();

        // Flash the page
        volatile uint32_t *dest = dest_page;
        const uint32_t *src = src_page;
        for (uint32_t i = 0; i < doublewords_per_page; i++) {
            const uint32_t low = *src++;
            const uint32_t high = *src++;

            SET_BIT(FLASH->CR, FLASH_CR_PG);
            *dest++ = low;
            *dest++ = high;
            if (!flash_wait_for_last_operation()) {
                return;
            }
            CLEAR_BIT(FLASH->CR, FLASH_CR_PG);
        }
        hal::watchdog::kick();
    }
}

[[nodiscard]] bool bootloader_is_up_to_date() {
    return std::memcmp(bootloader_region_start, bootloader_code, BOOTLOADER_SIZE) == 0;
}

} // namespace

void bootloader_update::run() {
    for (;;) {
        if (bootloader_is_up_to_date()) {
            rtt::print("bootloader is up-to-date\n");
            return;
        } else {
            rtt::print("updating bootloader\n");
            run_internal();
        }
    }
}

#else

void bootloader_update::run() {}

#endif
