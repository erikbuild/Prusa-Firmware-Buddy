#include <buddy/littlefs_internal.h>
#include <common/w25x.hpp>

using Storage = W25xFlash;

static Storage &storage_instance() {
    return Storage::instance();
}

static constexpr uint32_t block_size = Storage::block_size;
static constexpr uint32_t addr_offset = Storage::fs_start_address;

static lfs_t lfs;

// Read a region in a block. Negative error codes are propagated
// to the user.
static int read(const struct lfs_config *c, lfs_block_t block,
    lfs_off_t off, void *buffer, lfs_size_t size);

// Program a region in a block. The block must have previously
// been erased. Negative error codes are propagated to the user.
// May return LFS_ERR_CORRUPT if the block should be considered bad.
static int prog(const struct lfs_config *c, lfs_block_t block,
    lfs_off_t off, const void *buffer, lfs_size_t size);

// Erase a block. A block must be erased before being programmed.
// The state of an erased block is undefined. Negative error codes
// are propagated to the user.
// May return LFS_ERR_CORRUPT if the block should be considered bad.
static int erase(const struct lfs_config *c, lfs_block_t block);

// Sync the state of the underlying block device. Negative error codes
// are propagated to the user.
static int sync(const struct lfs_config *c);

struct LFSInternal {
#if PRINTER_IS_PRUSA_MINI()
    static constexpr lfs_size_t cache_size = 128; // need to save RAM on MINI, also PNG draw is faster due smaller and more efficient display
#else
    static constexpr lfs_size_t cache_size = 512; // PNG draws about 4% faster compared to cache_size 128
#endif
    static constexpr lfs_size_t lookahead_size = 16;

    std::byte read_buffer[cache_size];
    std::byte prog_buffer[cache_size];
    std::byte lookahead_buffer[lookahead_size];
};
static LFSInternal lfs_internal;

// configuration of the filesystem is provided by this struct
static struct lfs_config littlefs_config = {
    // block device operations
    .context = nullptr,
    .read = read,
    .prog = prog,
    .erase = erase,
    .sync = sync,

    // block device configuration
    .read_size = 1,
    .prog_size = 1,
    .block_size = block_size,
    .block_count = 0, // to be initialized at runtime
    .block_cycles = 500,
    .cache_size = lfs_internal.cache_size,
    .lookahead_size = lfs_internal.lookahead_size,
    .read_buffer = &lfs_internal.read_buffer,
    .prog_buffer = &lfs_internal.prog_buffer,
    .lookahead_buffer = &lfs_internal.lookahead_buffer,
    .name_max = 0,
    .file_max = 0,
    .attr_max = 0,
    .metadata_max = 0,
};

static uint32_t get_address(lfs_block_t block, lfs_off_t offset) {
    return addr_offset + block * littlefs_config.block_size + offset;
}

static bool address_is_valid(uint32_t address) {
    return address >= addr_offset && address <= (storage_instance().block_count() * block_size);
}

static int read([[maybe_unused]] const struct lfs_config *c, lfs_block_t block,
    lfs_off_t off, void *buffer, lfs_size_t size) {

    uint32_t addr = get_address(block, off);
    if (!address_is_valid(addr)) {
        return LFS_ERR_INVAL;
    }

    auto &storage = storage_instance();
    storage.read(addr, static_cast<uint8_t *>(buffer), size);

    if (storage.fetch_error() != 0) {
        return LFS_ERR_IO;
    }

    return 0;
}

static int prog([[maybe_unused]] const struct lfs_config *c, lfs_block_t block,
    lfs_off_t off, const void *buffer, lfs_size_t size) {

    uint32_t addr = get_address(block, off);
    if (!address_is_valid(addr)) {
        return LFS_ERR_INVAL;
    }

    auto &storage = storage_instance();
    storage.program(addr, (uint8_t *)buffer, size);

    if (storage.fetch_error() != 0) {
        return LFS_ERR_IO;
    }

    return 0;
}

static int erase([[maybe_unused]] const struct lfs_config *c, lfs_block_t block) {
    uint32_t addr = get_address(block, 0);
    if (!address_is_valid(addr)) {
        return LFS_ERR_INVAL;
    }

    auto &storage = storage_instance();
    storage.erase_block(addr);

    if (storage.fetch_error() != 0) {
        return LFS_ERR_IO;
    }

    return 0;
}

static int sync(__attribute__((unused)) const struct lfs_config *c) {
    return 0;
}

lfs_t *littlefs_internal_init() {
    // setup flash size
    littlefs_config.block_count = storage_instance().block_count() - (addr_offset / block_size);

    // mount the filesystem
    int err = lfs_mount(&lfs, &littlefs_config);

    // reformat if we can't mount the filesystem
    // this should only happen on the first boot
    if (err) {
        err = lfs_format(&lfs, &littlefs_config);
        if (err) {
            return NULL;
        }
        err = lfs_mount(&lfs, &littlefs_config);
        if (err) {
            return NULL;
        }
    }

    return &lfs;
}
