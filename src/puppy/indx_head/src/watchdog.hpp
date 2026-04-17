/// @file
#pragma once

#include <cstdint>
#include <utils/badge.hpp>
#include <utils/storage/single_linked_list.hpp>
#include <utils/uncopyable.hpp>

/// Use this macro to conveniently loop forever, while also declaring
/// a subsystem and kicking the watchdog for each iteration.
#define FOREVER_WITH_WATCHDOG(expire_ticks) \
    for (watchdog::Subsystem wd { expire_ticks };; wd.kick())

namespace watchdog {

using Ticks = std::uint16_t;

/// Represents a single subsystem to be watched.
/// Each subsystem may specify different timing requirements.
class Subsystem : Uncopyable {
private:
    const Ticks reset_value;
    Ticks down_counter;
    Subsystem *next;
    friend class Manager;

public:
    /// Create a subsystem, registering it to the manager.
    /// You may specify how often the subsystem needs to be kicked.
    explicit Subsystem(Ticks expire_ticks);

    /// Destroy subsystem, unregistering it from the manager.
    ~Subsystem();

    /// Refresh the subsystem.
    void kick();
};

/// Manager for subsystems.
class Manager : Uncopyable {
private:
    SingleLinkedList<&Subsystem::next> subsystems;

public:
    /// Register subsystem to the manager.
    /// Call this once for some subsystem.
    void add_subsystem(Badge<Subsystem>, Subsystem &);

    /// Unregister subsystem from the manager.
    /// Call this once for already-registered subsystem.
    void remove_subsystem(Badge<Subsystem>, Subsystem &);

    /// Decrement counter for all the registered subsystems.
    /// Kicks the hardware watchdog if none of the counters reached zero.
    /// Call this with frequency of the underlying hardware watchdog.
    void tick();
};

/// Returns single global instance of the manager, properly initialized.
Manager &manager();

} // namespace watchdog
