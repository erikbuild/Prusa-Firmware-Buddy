/// @file
/// Driver for the W25xxx family of SPI NOR flash memories.
///
/// SPI bus communication is handled by SpiFlashBus (spi_flash_bus.hpp).
#pragma once

#include <common/Pin.hpp>
#include <cstdint>
#include <device/board.h>

class SpiFlashBus;

class W25xFlash {
public:
    /// Smallest erasable unit. Called "sector" in W25Q NOR flash terminology,
    /// but we use "block" to match littlefs and NAND conventions.
    static constexpr uint32_t block_size = 4096;
    static constexpr uint32_t block64_size = 0x10000;

    // Address layout constants
    static constexpr uint32_t dump_start_address = 0;
#if BOARD_IS_BUDDY()
    // Some MINIes have 1MB flash, some have 8M
    // 49 = 196KiB offset for crash dump
    static constexpr uint32_t error_start_address = 49 * block_size;
    static constexpr uint32_t pp_start_address = 50 * block_size;
    static constexpr uint32_t fs_start_address = 51 * block_size;
#elif BOARD_IS_XBUDDY()
    // 8M = 2K of 4K blocks
    // 65 = 260KiB offset for crash dump
    static constexpr uint32_t error_start_address = 65 * block_size;
    static constexpr uint32_t pp_start_address = 66 * block_size;
    static constexpr uint32_t fs_start_address = 67 * block_size;
#elif BOARD_IS_XLBUDDY()
    // 8M = 2K of 4K blocks
    // 65 = 260KiB offset for crash dump, which is the total RAM size
    static constexpr uint32_t error_start_address = 65 * block_size;
    static constexpr uint32_t pp_start_address = 66 * block_size;
    static constexpr uint32_t fs_start_address = 68 * block_size;
#else
    #error "Unsupported board type"
#endif

    static constexpr uint32_t pp_size = fs_start_address - pp_start_address;
    static constexpr uint32_t dump_size = error_start_address - dump_start_address;

    static W25xFlash &instance();

    /// Initialize while the scheduler is running.
    ///
    /// When initialized using this function, all interface functions
    /// must be called in task context only.
    void init();

    /// (Re)initialize after the scheduler has been stopped.
    ///
    /// Call this once to abort ongoing transfers and take over the chip.
    /// This should only be used to write crash dump to external flash.
    ///
    /// All interface functions can be called in any context but are not reentrant.
    /// If reinitialized during DMA transfer it is aborted. If some
    /// data is already transfered to the chip at that point those data are
    /// written gracefully. If erase operation is ongoing it is completed
    /// during reinitialization.
    ///
    /// Worst case runtime is 100 seconds if called just after chip erase
    /// operation has been started. Worst case runtime is 200 seconds for
    /// maliciously crafted w25x responses.
    ///
    /// @retval true on success
    /// @retval false otherwise.
    bool reinit_before_crash_dump();

    /// Read data from the flash.
    /// Errors can be checked (and cleared) using fetch_error()
    void read(uint32_t addr, uint8_t *data, uint32_t len);

    /// Write data to the flash (the sector has to be erased first).
    /// Errors can be checked (and cleared) using fetch_error()
    void program(uint32_t addr, const uint8_t *data, uint32_t len);

    /// Erase single sector (4KB) of the flash.
    /// Errors can be checked (and cleared) using fetch_error()
    void erase_block(uint32_t addr);

    /// Erase block of 32KB of the flash.
    /// Errors can be checked (and cleared) using fetch_error()
    void block32_erase(uint32_t addr);

    /// Erase block of 64KB of the flash.
    /// Errors can be checked (and cleared) using fetch_error()
    void block64_erase(uint32_t addr);

    uint32_t erase_block_size() const { return block_size; }

    /// Return the number of available sectors.
    uint32_t block_count() const;

    uint32_t capacity() const { return erase_block_size() * block_count(); }

    /// Fetch and clear error of a previous operation.
    /// Returns 0 if there hasn't been any error.
    int fetch_error();

private:
    SpiFlashBus &bus;
    const buddy::hw::OutputPin &cs;

    /// To avoid deadlock erase_mutex must be always acquired
    /// earlier than communication_mutex and communication_mutex
    /// must be always released earlier than erase_mutex.
    ///
    /// Chip erase operation:
    ///   erase_mutex acquired, then communication_mutex acquired.
    ///   Do whole chip erase (can not be suspended).
    ///   communication_mutex released first, then erase_mutex.
    ///
    /// Block erase operation:
    ///   erase_mutex acquired, then communication_mutex acquired.
    ///   is_erasing set to true, start erase.
    ///   communication_mutex released (bus free while chip erases).
    ///   communication_mutex re-acquired to poll status.
    ///   When done, is_erasing set to false.
    ///   communication_mutex released first, then erase_mutex.
    ///
    /// Read/write standard priority:
    ///   erase_mutex acquired, then communication_mutex acquired.
    ///   Do read/write operation.
    ///   communication_mutex released first, then erase_mutex.
    ///
    /// Write high priority (multiple blocks at once):
    ///   Try to acquire erase_mutex.
    ///   Acquire communication_mutex and check is_erasing.
    ///   If is_erasing, pause erase operation.
    ///   Do read/write operation.
    ///   If is_erasing, resume erase operation.
    ///   communication_mutex released first.
    ///   erase_mutex (if acquired) released after.
    osStaticMutexDef_t erase_mutex_cb;
    osMutexDef_t erase_mutex_def;
    osMutexId erase_mutex = nullptr;
    osStaticMutexDef_t communication_mutex_cb;
    osMutexDef_t communication_mutex_def;
    osMutexId communication_mutex = nullptr;
    bool was_initialized = false;

    /// Block erase operation is in progress.
    ///
    /// Can be modified only when both erase_mutex and communication_mutex
    /// are acquired. Can be read only when communication_mutex is acquired.
    ///
    /// Needs to be checked only when erase_mutex was not acquired
    /// successfully, otherwise holding erase_mutex is sufficient to be
    /// sure erasing is not in progress.
    ///
    /// This exists because both mutexes must be held to start an erase.
    /// The erase implementation sets this to true once it acquires both.
    /// This ensures a high priority read/write doesn't assume erase is
    /// already ongoing if it fails to acquire erase_mutex and acquires
    /// communication_mutex in the moment erase acquired erase_mutex but
    /// didn't acquire communication_mutex yet.
    bool is_erasing = false;

    uint8_t device_id = 0;

    W25xFlash(SpiFlashBus &bus, const buddy::hw::OutputPin &cs);

    void select();
    void deselect();
    void write_enable();
    uint8_t read_status1();
    uint8_t read_status2();
    bool is_suspended();
    bool wait_busy();
    bool wait_erase();
    void erase(uint8_t cmd, uint32_t addr);
    bool check_id(uint8_t *out_devid);
    bool init_internal();

    void program_page(uint32_t addr, const uint8_t *data, uint16_t cnt, bool high_priority);
    void split_page_program(uint32_t addr, const uint8_t *data, uint32_t cnt, bool high_priority);
    void suspend_erase();
    void resume_erase();
};
