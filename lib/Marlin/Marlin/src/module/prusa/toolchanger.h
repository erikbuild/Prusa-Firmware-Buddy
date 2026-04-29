#pragma once
#include <bitset>
#include <inc/MarlinConfigPre.h>
#include <option/has_toolchanger.h>
#include <tool_index.hpp>
#include <utils/variant_utils.hpp>
#include <option/has_indx.h>
#include <option/has_tool_crash_recovery.h>

#if HAS_TOOLCHANGER()
    #include "toolchanger_utils.h"
    #include "bsod.h"
    #include <puppies/PuppyModbus.hpp>
    #if HAS_INDX()
        #include <fsm/nozzle_mismatch_phases.hpp>
    #endif

    #include <module/motion.h>

class PrusaToolChanger : public PrusaToolChangerUtils {
public:
    PrusaToolChanger()
        : PrusaToolChangerUtils() {
    }

    /**
     * @brief Perform a tool-change.
     * It may result in moving the previous tool out of the way and the new tool into place.
     * @warning Only run this from Marlin thread.
     * @param new_tool marlin id of new tool [indexed from 0]
     * @param return_type whether to return to previous position
     * @param return_position where to return to, needed for Z return even if no_return
     * @param z_lift select if printer should do Z lift before moving
     * @param z_return when true, printer will go to return_position.z after toolchange is complete, false will leave Z in last state (possibly lifted by z_lift)
     * @return true if toolchange was successful
     */
    [[nodiscard]] bool tool_change(const std::variant<PhysicalToolIndex, NoTool> new_tool, tool_return_t return_type, xyz_pos_t return_position, tool_change_lift_t z_lift = tool_change_lift_t::full_lift, bool z_return = true);

    #if HAS_TOOL_CRASH_RECOVERY()

    /// Structure to remember wanted toolchange result in case of a crash
    struct PrecrashData {
        uint8_t tool_nr; ///< Marlin id of last requested tool [indexed from 0] (last requested, not the tool physically picked)
        tool_return_t return_type; ///< Last wanted return position

        /**
         * @brief Destination to return to.
         * Linked to return_type.
         * @warning This is logical position! Use return_pos = toLogical(current_position).
         */
        XYZval<float, LogicalPosTag> return_pos;
    };

    /**
     * @brief Get last wanted state.
     * To be used in tool_change() in tool failure recovery.
     * @return last requested result of a toolchange
     */
    const PrecrashData &get_precrash() const {
        return precrash_data;
    }

    /**
     * @brief Force precrash state.
     * This is to be used when recovering from powerpanic through toolcrash.
     * @param data wanted result of a toolchange
     */
    void set_precrash_state(const PrecrashData &data) {
        precrash_data = data;
    }

    /**
     * @brief During toolcrash or toolfall recovery deselect dwarf as if all were parked.
     * @warning Only run this from Marlin thread.
     */
    void crash_deselect_dwarf();

    /**
     * @brief Disable loop() with automatic toolchange and toolfall detection.
     */
    void toolcheck_disable() {
        if (block_tool_check.exchange(true)) { // Test if already blocked
            bsod("Toolchange loop() already blocked");
        }
    }

    /**
     * @brief Reenable loop() with automatic toolchange and toolfall detection.
     */
    void toolcheck_enable();

    #endif // HAS_TOOL_CRASH_RECOVERY()

    /**
     * @brief Returns calibrated value with the tools docking position
     *
     * @param tool_nr number of tool
     */
    const xy_float_t get_tool_dock_position(PhysicalToolIndex tool);

    /**
     * @brief Loop that checks toolchanger state.
     * @warning Called only directly from marlin server.
     * @param printing true if currently printing, to not start toolchange spontaneously
     * @param paused true if currently paused (not printing), to not start toolchange spontaneously
     */
    void loop(bool printing, bool paused);

    /**
     * @brief Move to a XY position
     * @param x move to this x [mm]
     * @param y move to this y [mm]
     * @param feedrate use this feedrate [mm/s]
     */
    static void move(const float x, const float y, const feedRate_t feedrate);

    /**
     * @brief Bail out this toolchange fast
     *
     * Used from self-test when moves are stooped to avoid error report from toolchanger.
     *
     * This is sad and desperate way to avoid errors from quick stopped moves during toolchange. Consider situation:
     * - Selftest runs some operation that requires toolchange.
     *   (It cannot call it directly, but rather schedules a gcode to call it async)
     * - Toolchange is on default's task stack
     * - Toolchange schedules a move
     * - Schedule move gest on the default's task stack
     * - Move calls idle
     * - Now selftest button response is on the top of default's stack
     * - Response is abort (we want to cancel everything, disable steppers)
     *
     * There seems to be no nice way how to communicate the quick stop was executed as response to abort.
     * As the toolchange is already being run on the same stack as the response handling, it is already in the middle
     * of the operation. The Abort handling cannot just wait for it to finish as it is being run on the same stack.
     * Here a mature cooperative planning framework would terminate the toolchange task, but we are not that far.
     * Instead we set a bool tell the toolchange it is ok to terminate now without errors.
     */
    inline void quick_stop() {
        quick_stopped = true;
    }

    /**
     * @brief Know if it is safe to move in X and Y.
     * @return true if X and Y are homed
     */
    [[nodiscard]] bool can_move_safely(AxisHomeLevel required_level = AxisHomeLevel::full);

    #if HAS_INDX()
    /**
     * @brief Try to pick any enabled tool, iterating through all of them.
     * Useful when we need some tool picked but don't care which one.
     * @param return_type whether to return to previous position
     * @param return_position where to return to
     * @param z_lift select if printer should do Z lift before moving
     * @param z_return when true, printer will go to return_position.z after toolchange
     * @return true if any tool was successfully picked
     */
    [[nodiscard]] bool pick_any_tool(tool_return_t return_type, xyz_pos_t return_position, tool_change_lift_t z_lift = tool_change_lift_t::full_lift, bool z_return = true);

    /**
     * @brief Ensure head locking mechanism is open.
     * Checks head_open flag; if not open, calls open_head().
     * @param tool tool whose dock position to use; NoTool defaults to tool 0
     * @return true if head was already open or open head procedure was successful
     */
    bool ensure_head_open(std::variant<PhysicalToolIndex, NoTool> tool = NoTool {});

    enum class BumpError : uint8_t {
        unsafe_move,
        hit
    };

    /**
     * @brief Try bumping to a dock position to see if its empty.
     * @param tool this tool
     * @return true if empty
     */
    std::expected<void, BumpError> bump_to_dock(PhysicalToolIndex tool);

    /**
     * @brief Updates/invalidates the last picked tool (only when not printing if not overriden)
     * @param tool Which tool to write
     * @param override_always do this even if we are printing
     */
    void persist_last_picked_tool(std::variant<PhysicalToolIndex, NoTool> tool, bool override_always = false);

    /**
     * @brief Check nozzle presence against EEPROM last picked tool.
     *
     * - EEPROM says no tool but nozzle detected -> warn user (tool unexpectedly present)
     * - EEPROM says tool picked but no nozzle detected -> auto-correct to no tool
     *
     * @warning Must be called from the main (default) thread after puppies are ready.
     */
    void check_nozzle_presence_vs_eeprom();

    /**
     * @brief Periodically verify the picked tool's nozzle is still present during print.
     *
     * Compares the in-RAM active tool against the nozzle presence sensor. On mismatch,
     * pauses the print and opens the NozzleMismatch FSM. EEPROM is intentionally not
     * consulted here — it is invalidated during prints.
     *
     * Self-throttled to PRINT_NOZZLE_CHECK_PERIOD_MS; safe to call every cycle.
     * @warning Must be called from the main (default) thread, with block_tool_check clear.
     */
    void check_nozzle_presence_during_print();

    /// @brief Disable nozzle presence checking (e.g. during dock calibration)
    void set_nozzle_check_disabled(bool disabled) { nozzle_check_disabled = disabled; }
    bool is_nozzle_check_disabled() const { return nozzle_check_disabled; }

    uint16_t get_pickup_fail_count() const { return pickup_fail_count; }
    uint16_t get_park_fail_count() const { return park_fail_count; }

    #else
    /**
     * @brief Align the tool locks by bumping the right edge of the printer.
     * @warning Needs to be run from homing routine where motor currents and feedrates are set.
     * @note Needs homing after.
     * @note Does nothing if tool is picked.
     * @return true on success
     */
    [[nodiscard]] bool align_locks();
    #endif
private:
    #if HAS_TOOL_CRASH_RECOVERY()
    PrecrashData precrash_data = {}; ///< Remember wanted toolchange result in case of a crash
    uint8_t tool_check_fails = 0; ///< Count before toolfall
    static constexpr uint8_t TOOL_CHECK_FAILS_LIMIT = 3; ///< Limit of tool_check_fails before toolfall
    #endif
    std::atomic<bool> block_tool_check = false; ///< When true, block toolchange recursion and (on Dwarf) loop() with toolfall detection
    std::atomic<bool> quick_stopped = false; ///< The current toolchange was quick-stopped

    /**
     * @brief Ensure that X and Y are homed to be able to pick/park.
     * @return true on success, false if move is not safe after an attempt to home
     */
    [[nodiscard]] bool ensure_safe_move();

    #if HAS_INDX()
    bool head_open = false; ///< True when head locking mechanism is known to be open
    bool nozzle_check_disabled = false; ///< When true, skip nozzle presence vs EEPROM check (session-only, resets on reboot)
    std::atomic<uint16_t> pickup_fail_count = 0; ///< Number of failed pickup attempts since boot
    std::atomic<uint16_t> park_fail_count = 0; ///< Number of failed park attempts since boot

    static constexpr uint32_t PRINT_NOZZLE_CHECK_PERIOD_MS = 5000; ///< Period of in-print nozzle presence check
    uint32_t last_print_nozzle_check_ms = 0; ///< Tick of last in-print nozzle presence check

    /**
     * @brief Invalidate XY homing state, forcing a rehome before next toolchange.
     */
    void invalidate_xy_homing();

    /**
     * @brief If the given tool is at a position that would interfere with X homing,
     * move the head out of the way first.
     * @param tool the tool currently picked
     */
    void ensure_tool_homeable(PhysicalToolIndex tool);

    /**
     * @brief Open the head locking mechanism.
     * Moves to dock, wiggles E to align teeth, then fully unlocks.
     * @param tool tool whose dock position to use
     */
    void open_head(PhysicalToolIndex tool);

    /**
     * @brief Wiggle E to align teeth, then partial unlock.
     * Shared by open_head and park_procedure. Must be called with EMotorGuard active.
     */
    void wiggle_and_partial_unlock();

    /**
     * @brief Verify nozzle presence/absence.
     * @param prev_tool index of previous tool (for logs)
     * @param expect_present true after pickup (expect nozzle), false after park (expect no nozzle)
     * @return true if nozzle state matches expectation
     */
    [[nodiscard]] bool verify_nozzle_state(PhysicalToolIndex prev_tool, bool expect_present);

    enum class ToolchangeFailureAction { abort, retry };

    /**
     * @brief Show toolchange failure dialog and get user decision.
     * @param main_phase the primary Ignore/Retry/Abort prompt
     * @param confirm_abort_phase the abort confirmation prompt
     * @param confirm_retry_phase the retry confirmation prompt
     * @return user's chosen action
     */
    [[nodiscard]] ToolchangeFailureAction handle_toolchange_failure(
        PhaseNozzleMismatch main_phase,
        PhaseNozzleMismatch confirm_abort_phase,
        PhaseNozzleMismatch confirm_retry_phase);

    [[nodiscard]] ToolchangeFailureAction handle_pickup_failure();
    [[nodiscard]] ToolchangeFailureAction handle_park_failure();

    /**
     * @brief Apply common failure side-effects and return the action for caller control flow.
     */
    ToolchangeFailureAction apply_failure_action(ToolchangeFailureAction action);

    /**
     * @brief Pickup tool.
     * @param tool this tool
     * @return true on success
     */
    [[nodiscard]] bool pickup(PhysicalToolIndex tool);

    /**
     * @brief Execute the physical pickup sequence: move to dock, lock nozzle, exit.
     * Does not commit tool state — caller is responsible for state management.
     * @param tool this tool
     * INDX_TODO: Do homing moves to makes sure we dont crash and crash the printer
     */
    bool pickup_procedure(PhysicalToolIndex tool);

    /**
     * @brief Park tool.
     * @param tool this tool
     * @return true on success
     */
    [[nodiscard]] bool park(PhysicalToolIndex tool);

    /**
     * @brief Execute the physical park sequence: move to dock, unlock, release nozzle, exit.
     * Does not commit tool state — caller is responsible for state management.
     * @param tool this tool
     * INDX_TODO: Do homing moves to makes sure we dont crash and crash the printer
     */
    bool park_procedure(PhysicalToolIndex tool);
    #else
    /**
     * @brief Check if powerpanic happened.
     * @return true if powerpanic happened and toolchange has to quit immediately
     */
    [[nodiscard]] bool check_emergency_stop();

    /**
     * @brief Pickup tool.
     * @param dwarf this tool
     * @return true on success
     */
    [[nodiscard]] bool pickup(buddy::puppies::Dwarf &dwarf);

    /**
     * @brief Park tool.
     * @param dwarf this tool
     * @return true on success
     */
    [[nodiscard]] bool park(buddy::puppies::Dwarf &dwarf);

    /**
     * @brief Purges tool by extruding some filament outside of print area and tries to shake it away and wipe it by parking
     */
    bool purge_tool(buddy::puppies::Dwarf &dwarf);

    /**
     * @brief Check if steps were skipped during parking.
     * If so, reset crash_s state and do homing.
     * @return true on success, false if rehoming failed
     */
    bool check_skipped_step();

    /**
     * @brief When crash happens during toolchange, triggers toolchanger recovery sequence.
     * Called from tool_change().
     */
    void toolcrash();

    /**
     * @brief When printing and tool falls off, triggers toolchanger recovery sequence.
     * Called from marlin server loop() task.
     */
    void toolfall();

    #endif

    /**
     * @brief Plan a smooth unparking move towards destination
     * unpark_to() should only be called after pickup() in order to plan a smooth unpark move,
     * mirroring the park operation. Such move only makes sense if the toolhead hasn't stopped
     * for other operations, such as Z compensation.
     * @param destination
     */
    void unpark_to(const xy_pos_t &destination);

    /**
     * @brief Compensate the Z offset by the specified amount
     */
    void z_shift(const float diff);
};

extern PrusaToolChanger prusa_toolchanger;

#endif
