#include <puppies/puppy_task.hpp>

#include "Marlin/src/module/prusa/toolchanger.h"
#include "Marlin/src/module/stepper.h"
#include <buddy/bootstrap_state.hpp>
#include <cmsis_os.h>
#include <common/bsod.h>
#include <common/timing.h>
#include <freertos/timing.hpp>
#include <logging/log.hpp>
#include <option/has_toolchanger.h>
#include <tasks.hpp>

#include <option/has_dwarf.h>
#if HAS_DWARF()
    #include <puppies/Dwarf.hpp>
#endif

#include <option/has_indx_head.h>
#if HAS_INDX_HEAD()
    #include <puppies/INDX.hpp>
#endif

#include <option/has_ac_controller.h>
#if HAS_AC_CONTROLLER()
    #include <puppies/ac_controller.hpp>
#endif

#include <option/has_tool_offset_sensor.h>
#if HAS_TOOL_OFFSET_SENSOR()
    #include <puppies/tool_offset_sensor.hpp>
#endif

#include <option/has_anfc.h>
#if HAS_ANFC()
    #include <anfc/modbus.hpp>
    #include <feature/openprinttag/request_manager.hpp>
#endif

#include <option/has_puppy_modularbed.h>
#if HAS_PUPPY_MODULARBED()
    #include <puppies/modular_bed.hpp>
#endif

#include <option/has_xbuddy_extension.h>
#if HAS_XBUDDY_EXTENSION()
    #include <puppies/xbuddy_extension_bootstrap.hpp>
    #include <puppies/xbuddy_extension.hpp>
#endif

#include <option/has_mmu2.h>
#if HAS_MMU2()
    #include <puppies/mmu.hpp>
#endif

#include <option/has_puppy_bootstrap.h>
#if HAS_PUPPY_BOOTSTRAP()
    #include <puppies/PuppyBootstrap.hpp>
#endif

/// Set this to true when debugging puppy task issues.
///
/// This is useful especially for debugging XBE crashes, without this the puppy
/// task would reset running puppy and retry the bootstrap procedure, making
/// the debugging session impossible.
///
/// MAKE SURE TO ALWAYS REVERT THIS TO FALSE BEFORE MERGING!
#define PUPPY_TASK_DEBUG() false

LOG_COMPONENT_DEF(Puppies, logging::Severity::debug);

namespace buddy::puppies {

#if HAS_ANFC()
namespace {
    class AnfcModbusClient final : public anfc::modbus::Client {
    private:
        PuppyModbus &bus;

    public:
        explicit AnfcModbusClient(PuppyModbus &bus)
            : bus { bus } {}

        bool read(anfc::Device device, anfc::modbus::Event &event) final {
            return bus.read_input_registers(anfc::modbus::server_address(device), event);
        }

        bool write(anfc::Device device, const anfc::modbus::Request &request) final {
            return bus.write_holding_registers(anfc::modbus::server_address(device), request);
        }

        bool write(anfc::Device device, const anfc::modbus::AcceptEvent &accept_event) final {
            return bus.write_holding_registers(anfc::modbus::server_address(device), accept_event);
        }
    };
} // namespace
#endif

static std::atomic<bool> stop_request = false; // when this is set to true, puppy task will gracefully stop its execution

#if HAS_PUPPY_BOOTSTRAP()
static PuppyBootstrap::BootstrapResult bootstrap_puppies(PuppyBootstrap::BootstrapResult minimal_config, [[maybe_unused]] PuppyModbus &bus) {
    // boostrap first
    log_info(Puppies, "Starting bootstrap");
    PuppyBootstrap puppy_bootstrap { PuppyModbus::share_buffer() };
    return puppy_bootstrap.run(minimal_config);
}
#endif

static void puppy_run_timeout() {
#if PUPPY_TASK_DEBUG()
    // #error dead code found by automatic analyses (see BFW-5461)
    log_error(Puppies, "ErrCode::ERR_SYSTEM_PUPPY_RUN_TIMEOUT");
    osDelay(1000);
#else
    fatal_error(ErrCode::ERR_SYSTEM_PUPPY_RUN_TIMEOUT);
#endif
}

#if HAS_XBUDDY_EXTENSION()
static bool xbe_requires_reset = false;
static void xbuddy_extension_verify_running(PuppyModbus &bus) {
    // usually takes 50ms but let's be generous
    constexpr auto timeout_ms = 200;

    const auto start_ms = ticks_ms();
    for (;;) {
        if (xbuddy_extension.ping(bus) != CommunicationStatus::ERROR) {
            return;
        }
        if (ticks_diff(ticks_ms(), start_ms) > timeout_ms) {
            puppy_run_timeout();
        }
    }
}
#endif

#if HAS_PUPPY_BOOTSTRAP()
static void verify_puppies_running(PuppyModbus &bus) {
    // wait for all the puppies to be reacheable
    log_info(Puppies, "Waiting for puppies to boot");

    auto err_supressor = PuppyModbus::ErrorLogSupressor();

    constexpr uint32_t WAIT_TIME = 5000;
    auto reacheability_wait_start = ticks_ms();
    do {
        bool modular_bed_ok = true;
    #if HAS_PUPPY_MODULARBED()
        modular_bed_ok = !modular_bed.is_enabled() || (modular_bed.ping(bus) != CommunicationStatus::ERROR);
    #endif

        uint8_t num_dwarfs_ok = 0, num_dwarfs_dead = 0;
    #if HAS_DWARF()
        for (Dwarf &dwarf : dwarfs) {
            if (dwarf.is_enabled()) {
                if (dwarf.ping(bus) != CommunicationStatus::ERROR) {
                    ++num_dwarfs_ok;
                } else {
                    ++num_dwarfs_dead;
                }
            }
        }
    #endif
        bool indx_head_ok = true;
    #if HAS_INDX_HEAD()
        indx_head_ok = buddy::puppies::indx.ping(bus) != CommunicationStatus::ERROR;
    #endif

        if (num_dwarfs_dead == 0 && modular_bed_ok && indx_head_ok) {
            log_info(Puppies, "All puppies are reacheable. Continuing");
            return;
        } else if (ticks_diff(reacheability_wait_start + WAIT_TIME, ticks_ms()) > 0) {
            log_info(Puppies, "Puppies not ready (dwarfs_num: %d/%d, bed: %i, indx: %i), waiting another 200 ms",
                num_dwarfs_ok, num_dwarfs_ok + num_dwarfs_dead, static_cast<int>(modular_bed_ok), static_cast<int>(indx_head_ok));
            osDelay(200);
            continue;
        } else {
            puppy_run_timeout();
        }
    } while (true);
}
#endif

static void puppy_task_loop(PuppyModbus &bus) {
#if HAS_TOOLCHANGER() && HAS_DWARF()
    size_t slow_stage = 0; ///< Switch slow action
#endif

    // periodically update puppies until there is a failure
    while (true) {
        if (stop_request) {
            return;
        }

        [[maybe_unused]] uint32_t cycle_ticks = ticks_ms(); ///< Only one tick read per cycle, value will be reused by last_ticks_ms()
        // One slow action
        bool worked = false;

#if HAS_TOOLCHANGER() && HAS_DWARF()
        if (!prusa_toolchanger.update(bus)) {
            return;
        }
        // Get dwarf that is selected
        // The source variable is set in this thread in prusa_toolchanger.update() called above, so no race
        auto &active = prusa_toolchanger.getActiveToolOrFirst(); ///< Currently selected dwarf

        // Fast fifo pull from selected dwarf
        if (active.is_selected()) {
            bool more = true; ///< Pull while there is something in fifo
            // Pull fifo only this many times
            for (int active_fifo_attempts = 5; more && active_fifo_attempts > 0; active_fifo_attempts--) {
                if (active.pull_fifo(bus, more) == CommunicationStatus::ERROR) {
                    return;
                }
            }
        } else {
            osDelay(1); // No dwarf is selected, wait a bit
        }

        size_t orig_stage = slow_stage;
        do {
            // Increment stage, there are 2 actions per dwarf and one modular bed
            if (++slow_stage >= (2 * std::size(dwarfs) + 1)) {
                slow_stage = 0;
            }

            if (slow_stage / 2 < std::size(dwarfs)) { // Two actions per dwarf
                auto &dwarf = dwarfs[slow_stage / 2];
                if (!dwarf.is_enabled()) {
                    continue; // skip if this dwarf is not enabled
                }

                if (slow_stage % 2) {
                    if (&active == &dwarf) {
                        continue; // Skip selected dwarf
                    }

                    // Fast refresh of non-selected dwarf
                    CommunicationStatus status = dwarf.fifo_refresh(bus, cycle_ticks);
                    if (status == CommunicationStatus::ERROR) {
                        return;
                    }
                    worked |= status == CommunicationStatus::OK;
                } else {
                    // Slow refresh of non-selected dwarf
                    CommunicationStatus status = dwarf.refresh(bus);
                    if (status == CommunicationStatus::ERROR) {
                        return;
                    }
                    worked |= status == CommunicationStatus::OK;
                }
            } else
#endif

#if HAS_PUPPY_MODULARBED()
            {
                // Try slow refresh of modular bed
                if (modular_bed.refresh(bus) == CommunicationStatus::ERROR) {
                    return;
                }
            }
#endif
#if HAS_INDX_HEAD()
            {
                bool more = true; ///< Pull while there is something in fifo
                // Pull fifo only this many times
                for (int active_fifo_attempts = 5; more && active_fifo_attempts > 0; active_fifo_attempts--) {
                    if (buddy::puppies::indx.pull_fifo(bus, more) == CommunicationStatus::ERROR) {
                        log_error(Puppies, "Loop exit: indx.pull_fifo() ERROR");
                        ++buddy::puppies::indx.fifo_error_count;
    #if !PUPPY_TASK_DEBUG()
                        return;
    #endif
                    }
                }

                // Try slow refresh of INDX
                CommunicationStatus status = indx.refresh(bus);
                if (status == CommunicationStatus::ERROR) {
                    log_error(Puppies, "Loop exit: indx.refresh() ERROR");
                    ++buddy::puppies::indx.refresh_error_count;
    #if !PUPPY_TASK_DEBUG()
                    return;
    #endif
                }

                worked |= status == CommunicationStatus::OK;
            }
#endif
#if HAS_XBUDDY_EXTENSION()
            {
                // TODO: Deal with possibility of extension being optional
                CommunicationStatus status = xbuddy_extension.refresh(bus);
                if (status == CommunicationStatus::ERROR) {
                    log_error(Puppies, "Loop exit: xbuddy_extension.refresh() ERROR");
                    ++xbuddy_extension.refresh_error_count;
    #if !PUPPY_TASK_DEBUG()
                    xbe_requires_reset = true;
                    return;
    #endif
                }

                worked |= status == CommunicationStatus::OK;
            }
#endif
#if HAS_AC_CONTROLLER()
            {
                CommunicationStatus status = ac_controller.refresh(bus);
                if (status == CommunicationStatus::ERROR) {
                    return;
                }

                worked |= status == CommunicationStatus::OK;
            }
#endif
#if HAS_TOOL_OFFSET_SENSOR()
            {
                CommunicationStatus status = tool_offset_sensor.refresh(bus);
                if (status == CommunicationStatus::ERROR) {
                    return;
                }

                worked |= status == CommunicationStatus::OK;
            }
#endif
#if HAS_MMU2()
            {
                CommunicationStatus status = mmu.refresh(bus);
                if (status == CommunicationStatus::ERROR) {
                    // Actually, failed MMU communication is not an issue on this level.
                    // Timeout and retries are being handled on the protocol_logic level
                    // while MODBUS purely serves as a pass-through transport media
                    // -> no need to panic when MMU doesn't communicate now
                }

                worked |= status == CommunicationStatus::OK;
            }
#endif
#if HAS_ANFC()
            if (AnfcModbusClient client { bus }; buddy::openprinttag::manager().step(client)) {
                worked = true;
            } else {
                return;
            }
#endif

#if HAS_TOOLCHANGER() && HAS_DWARF()
        } while (!worked && slow_stage != orig_stage); // End if we did some work or if no stage has anything to do
#endif
        if (worked) {
            freertos::yield();
        } else {
            freertos::delay(1);
        }
    }
}

static bool puppy_initial_scan(PuppyModbus &bus) {
    // init each puppy
#if HAS_XBUDDY_EXTENSION()
    // TODO: Eventually, there'll be printers that have the extension as
    // optional at runtime - we'll have to deal with that somehow.
    if (xbuddy_extension.initial_scan(bus) == CommunicationStatus::ERROR) {
        xbe_requires_reset = true;
        return false;
    }
#endif

#if HAS_DWARF()
    for (Dwarf &dwarf : dwarfs) {
        if (dwarf.is_enabled()) {
            if (dwarf.initial_scan(bus) == CommunicationStatus::ERROR) {
                return false;
            }
        }
    }
#endif

#if HAS_INDX_HEAD()
    if (indx.initial_scan(bus) == CommunicationStatus::ERROR) {
        return false;
    }

#endif

#if HAS_PUPPY_MODULARBED()
    if (modular_bed.initial_scan(bus) == CommunicationStatus::ERROR) {
        return false;
    }
#endif

#if HAS_AC_CONTROLLER()
    if (ac_controller.initial_scan(bus) == CommunicationStatus::ERROR) {
        return false;
    }
#endif

#if HAS_TOOL_OFFSET_SENSOR()
    if (tool_offset_sensor.initial_scan(bus) == CommunicationStatus::ERROR) {
        return false;
    }
#endif

    return true;
}

#if HAS_AC_CONTROLLER()
[[nodiscard]] static bool wait_for_ac_controller(PuppyModbus &bus) {
    // AC controller is vital part of the printer, there is no upper limit
    // on how long we are willing to wait for the bootstrap.
    for (;;) {
        // At this point, puppy_task_loop() is not yet running, so we must
        // manually call refresh() on puppies. Without this, XBE can't make
        // progress while flashing/veryfing ACC. It would also stop sending
        // healthy heartbeats which would in turn put ACC into safe state.
        // We should run this as often as possible to minimize time when
        // XBE is waiting for firmware chunk.
        if (xbuddy_extension.refresh(bus) == CommunicationStatus::ERROR) {
            return false;
        }
        if (ac_controller.refresh(bus) == CommunicationStatus::ERROR) {
            return false;
        }
        using xbuddy_extension::NodeState;
        switch (ac_controller.get_node_state()) {
        case NodeState::unknown:
            bootstrap_state_set(0, BootstrapStage::ac_controller_unknown);
            break;
        case NodeState::verify:
            bootstrap_state_set(0, BootstrapStage::ac_controller_verify);
            break;
        case NodeState::flash:
            bootstrap_state_set(xbuddy_extension.get_flash_progress_percent(), BootstrapStage::ac_controller_flash);
            break;
        case NodeState::ready:
            bootstrap_state_set(0, BootstrapStage::ac_controller_ready);
            return true;
        }
    }
}
#endif

#if HAS_TOOL_OFFSET_SENSOR()
[[nodiscard]] static bool wait_for_tool_offset_sensor(PuppyModbus &bus) {
    // Tool offset sensor is vital part of the printer, there is no upper limit
    // on how long we are willing to wait for the bootstrap.
    for (;;) {
        // At this point, puppy_task_loop() is not yet running, so we must
        // manually call refresh() on puppies. Without this, XBE can't make
        // progress while flashing/veryfing TOOL_OFFSET_SENSOR. It would also stop sending
        // healthy heartbeats which would in turn put TOOL_OFFSET_SENSOR into safe state.
        // We should run this as often as possible to minimize time when
        // XBE is waiting for firmware chunk.
        if (xbuddy_extension.refresh(bus) == CommunicationStatus::ERROR) {
            return false;
        }
        if (tool_offset_sensor.refresh(bus) == CommunicationStatus::ERROR) {
            return false;
        }

        using xbuddy_extension::NodeState;
        switch (tool_offset_sensor.get_node_state()) {
        case NodeState::unknown:
            bootstrap_state_set(0, BootstrapStage::tool_offset_sensor_unknown);
            break;
        case NodeState::verify:
            bootstrap_state_set(0, BootstrapStage::tool_offset_sensor_verify);
            break;
        case NodeState::flash:
            bootstrap_state_set(xbuddy_extension.get_flash_progress_percent(), BootstrapStage::tool_offset_sensor_flash);
            break;
        case NodeState::ready:
            bootstrap_state_set(0, BootstrapStage::tool_offset_sensor_ready);
            return true;
        }
    }
}
#endif

#if HAS_INDX_HEAD()
[[nodiscard]] static bool indx_head_power_on(PuppyModbus &bus) {
    // This function only works for these printers.
    static_assert(PRINTER_IS_PRUSA_COREONE() || PRINTER_IS_PRUSA_COREONEL());

    // INDX head is vital part of the printer, there is no upper limit
    // on how long we are willing to wait for power-up.
    for (;;) {
        switch (xbuddy_extension.set_mmu_power(bus, true)) {
        case CommunicationStatus::OK:
            return true;
        case CommunicationStatus::ERROR:
            return false;
        case CommunicationStatus::SKIPPED:
            continue;
        }
    }
}
#endif

void run() {
    TaskDeps::wait(TaskDeps::Tasks::puppy_task_start);

    PuppyModbus &bus = buddy::puppies::puppyModbus;
    [[maybe_unused]] bool first_run = true;

#if HAS_PUPPY_BOOTSTRAP()
    // by default, we want one modular bed and one dwarf
    PuppyBootstrap::BootstrapResult minimal_puppy_config = PuppyBootstrap::MINIMAL_PUPPY_CONFIG;
#endif

    do {
#if HAS_XBUDDY_EXTENSION()
        if (first_run || xbe_requires_reset) {
            {
                BootloaderProtocol bootloader_protocol { PuppyModbus::share_buffer().data() };
                xbuddy_extension_bootstrap(bootloader_protocol);
            }
            xbuddy_extension_verify_running(bus);
        }
#endif

#if HAS_INDX_HEAD()
        if (first_run || xbe_requires_reset) {
            if (!indx_head_power_on(bus)) {
                break; // go to puppy recovery
            }
        }
#endif

#if HAS_XBUDDY_EXTENSION()
        xbe_requires_reset = false;
#endif

#if HAS_PUPPY_BOOTSTRAP()
        // reset and flash the puppies
        auto bootstrap_result = bootstrap_puppies(minimal_puppy_config, bus);
        // once some puppies are detected, consider this minimal puppy config (do no allow disconnection of puppy while running)
        minimal_puppy_config = bootstrap_result;

    #if HAS_PUPPY_MODULARBED()
        // set what puppies are connected
        modular_bed.set_enabled(bootstrap_result.is_dock_occupied(Dock::MODULAR_BED));
    #endif
    #if HAS_DWARF()
        for (const auto dwarf_dock : DWARFS) {
            auto dwarf_index = to_dwarf_index(dwarf_dock);
            if (dwarf_index < PhysicalToolIndex::count) {
                dwarfs[PhysicalToolIndex::from_raw(dwarf_index)].set_enabled(bootstrap_result.is_dock_occupied(dwarf_dock));
            } else {
                dwarfs[dwarf_index].set_enabled(false);
            }
        }
    #endif

        // wait for puppies to boot up, ensure they are running
        verify_puppies_running(bus);
#endif

        do {
            // do intial scan of puppies to init them
            if (!puppy_initial_scan(bus)) {
                break;
            }

#if HAS_DWARF()
            // select active tool (previously active tool, or first one when starting)
            if (!prusa_toolchanger.init(bus, first_run)) {
                log_error(Puppies, "Unable to select tool, retrying");
                break;
            }
#endif

#if HAS_AC_CONTROLLER()
            if (!wait_for_ac_controller(bus)) {
                break; // go to puppy recovery
            }
#endif

#if HAS_TOOL_OFFSET_SENSOR()
            if (!wait_for_tool_offset_sensor(bus)) {
                break; // go to puppy recovery
            }
#endif

            TaskDeps::provide(TaskDeps::Dependency::puppies_ready);
            first_run = false;
            log_info(Puppies, "Puppies are ready");

            TaskDeps::wait(TaskDeps::Tasks::puppy_run);

#if HAS_DWARF()
            // write current Marlin's state of the E TMC
            stepperE0.push();
#endif

            // now run puppy main loop
            puppy_task_loop(bus);
        } while (false);

        if (stop_request) {
            // stop of puppy task was requested, stop here gracefully, without holding any mutexes and such
            osThreadSuspend(nullptr);
        }

        log_error(Puppies, "Communication error, going to recovery puppies");
#if HAS_PUPPY_BOOTSTRAP()
        if (PuppyBootstrap::any_dock_supports_crash_dump()) {
            osDelay(1300); // give puppies time to finish dumping
        }
#endif
    } while (true);
}

void suspend_puppy_task() {
    // ask puppy thread to stop its execution
    stop_request = true;
}

} // namespace buddy::puppies
