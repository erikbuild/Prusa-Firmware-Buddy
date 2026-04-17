/// @file
#include "watchdog.hpp"

#include "critical_section.hpp"
#include "hal_watchdog.hpp"

namespace watchdog {

void Manager::tick() {
    CriticalSection cs;
    bool alive = true;
    for (auto &subsystem : subsystems) {
        if (subsystem.down_counter == 0) {
            alive = false;
        } else {
            --subsystem.down_counter;
        }
    }
    if (alive) {
        hal::watchdog::kick();
    }
}

void Manager::add_subsystem(Badge<Subsystem>, Subsystem &subsystem) {
    CriticalSection cs;
    subsystems.push_front(subsystem);
}

void Manager::remove_subsystem(Badge<Subsystem>, Subsystem &subsystem) {
    CriticalSection cs;
    subsystems.remove(subsystem);
}

Manager &manager() {
    static Manager instance;
    return instance;
}

Subsystem::Subsystem(Ticks expire_ticks)
    : reset_value { expire_ticks }
    , down_counter { expire_ticks }
    , next { nullptr } {
    manager().add_subsystem({}, *this);
}

Subsystem::~Subsystem() {
    manager().remove_subsystem({}, *this);
}

void Subsystem::kick() {
    CriticalSection cs;
    down_counter = reset_value;
}

} // namespace watchdog
