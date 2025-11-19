#pragma once

#include <cstdint>
#include <functional>
#include <atomic>

namespace device {

/**
 * @brief Class to gather watchdog kicks from multiple threads.
 * Each instance embeds itself to a static linked list.
 * Only if all instances kicked can the hardware watchdog be reloaded.
 */
class MultiWatchdog {
    static MultiWatchdog *list; ///< Beginning of the list
    static std::function<void(void)> refresh; ///< Function to refresh hardware watchdog, nullptr if init() was not called

    MultiWatchdog *next = nullptr; ///< Continuation of a list of all watchdog instances
    uint8_t mark = false; ///< This mark is nonzero if this instance was kicked

    /**
     * @brief Check the entire list and if all instances are kicked, reload hardware.
     */
    static void check_all();

public:
    /**
     * @brief Create an instance of a watchdog and add it to the global list.
     */
    MultiWatchdog();

    /**
     * @brief Watchdog cannot be destroyed.
     * The destructor is not deleted as watchdog instance can be created on stack,
     * but that function can never end.
     */
    ~MultiWatchdog();

    /**
     * @brief Initialize hardware watchdog.
     * This needs to be done only once for all instances.
     * @param init_hw function to initialize hardware watchdog
     * @param refresh_ function to refresh hardware watchdog
     */
    static void init(std::function<void(void)> init_hw, std::function<void(void)> refresh_);

    /**
     * @brief Refresh one instance of the watchdog.
     * @param hardware true to reset hardware, false to only mark (another instance must reset the hardware)
     */
    void kick(bool hardware = true);
};

/**
 * @brief This is a MultiWatchdog variant that allows longer time between kicks.
 * You have to call a tick() function in regular intervals that will kick the watchdog between calls to extended_kick().
 * You can set limit in tick(), and it will kick the watchdog for that specified time. It will stop after the limit expires.
 */
class MultiWatchdogExtended : public MultiWatchdog {
    std::atomic<uint32_t> extender = 0; ///< Counts ticks that can pass without kicking the watchdog

public:
    /**
     * @brief Kick and allow tick() to refresh the watchdog for some time.
     * @note This only marks (another instance must reset the hardware).
     * @note Not using override to avoid performance penalty.
     */
    void extended_kick();

    /**
     * @brief Count time between kicks and refresh the watchdog.
     * @param limit number of ticks that can pass without kicking the watchdog
     * @note This only marks (another instance must reset the hardware).
     */
    void tick(uint32_t limit);
};
} // namespace device
