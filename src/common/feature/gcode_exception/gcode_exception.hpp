/// @file
#pragma once

#include <cstdint>

#include <utils/uncopyable.hpp>
#include <utils/storage/single_linked_list.hpp>

class GCodeExceptionHandlerBase;

/// Manager for an alternative, "cooperative" exception system.
/// (Because standard C++ exception system is not enabled in the Buddy firmware.)
/// Its purpose is interrupting gcode excecution and movements, not error state handling in general.
///
/// When an "exception" is "thrown", it starts "unwinding" the stack, in the sense that the code execution continues,
/// but all actions are (resp. should be) discarded until the unwinding stops.
/// Additionally, there should be is_unwinding() checks scattered around the code to allow early exits from the procedures.
///
/// Contrary to the "standard" exception system, exceptions here are thrown "at" specific handlers ("try-catch blocks") that handle them.
/// This is more akin to break statements that allow breaking other than just innermost loops, for example like in the D language: https://dlang.org/spec/statement.html#BreakStatement
///
/// For usage example, see gcode_exception_example.cpp
class GCodeExceptionManager final {
    friend class GCodeExceptionHandlerBase;

public:
    /// "Throws" an "exception" that is to be handled by the specified @p handler.
    /// All printer moves are immediately stopped (quick_stop) - may result in skipped steps!
    /// Enters the "stack unwinding" state as described above - all printer actions are discarded till the unwinding stops.
    /// The unwinding is finished and standard execution is resumed at the end of the @p handler scope.
    /// Additionally, the handler callback is called (only this single handler).
    ///
    /// If @p nullptr is passed, the exception is considered "unhandled" and the unwinding needs to be stopped by calling @p finish_unwinding_unhandled_exception.
    void throw_at(GCodeExceptionHandlerBase *handler);

    /// See @p throw_at
    inline void throw_unhandled() {
        throw_at(nullptr);
    }

    /// Finishes unwinding an unhandled exception. Does nothing if we're not unwinding due to an unhandled exception.
    /// !!! Can only be called if there are no handlers on the stack.
    /// @returns @p true if there was an unhandled exception to be finished.
    bool finish_unwinding_unhandled_exception();

    /// To be called by the planner when enqueing a move that changes the XYZ position
    /// Used for asserting that we're not trying to do XYZ moves inside a handler that doesn't support it
    void report_xyz_move();

public:
    /// @returns true if we're currently "unwinding" the stack  due to an exception
    inline bool is_unwinding() const {
        return is_unwinding_;
    }

    /// @returns true if we're unwinding due to an unhandled exception
    inline bool is_unwinding_unhandled() const {
        return is_unwinding_unhandled_exception_;
    }

    /// @returns number of exceptions thrown since the printer start
    inline uint32_t throw_count() const {
        return throw_count_;
    }

private:
    /// Stops unwinding if there are no handlers being thrown at anymore. Does nothing otherwise.
    /// @returns if the manager changed from unwinding to stadnard operation.
    bool maybe_finish_unwinding();

    /// Updates cached data computed from handler configuration
    void update_handlers_cache();

private:
    static inline GCodeExceptionHandlerBase **next_handler(GCodeExceptionHandlerBase *);

    /// Currently active handlers
    SingleLinkedList<GCodeExceptionHandlerBase, next_handler> handlers_;

    /// Increased by 1 with every throw()
    uint32_t throw_count_ = 0;

    bool is_unwinding_ : 1 = false;

    bool is_unwinding_unhandled_exception_ : 1 = false;

    /// Whether any handler is active that does not support/allow XYZ moves
    bool can_move_xyz_ : 1 = true;
};

GCodeExceptionManager &gcode_exceptions();

enum class GCEHandlerExtent : uint8_t {
    /// The GCodeExceptionHandler is only intended for handling quickstops during extruder moves
    /// This is easier to handle, because quickstopping during extruder moves does not result in losing homing
    extruder_only,

    /// The GCodeExceptionHandler can recover from quick stopping even during printhead movements
    /// This requires way more consideration, as quickstopping such move can cause skipped steps and thus homing loss.
    /// And rehoming can result in origin shift.
    any_move,
};

class GCodeExceptionHandlerBase : public Uncopyable {
    friend class GCodeExceptionManager;

protected:
    explicit GCodeExceptionHandlerBase(GCEHandlerExtent extent);

    /// Finalizes the handler (to be called only in the destructor)
    /// @returns if finalizing the handler resulted in unwinding finish (and thus the handler callback should be called)
    bool finalize();

private:
    /// Next handler in the linked list
    GCodeExceptionHandlerBase *next_handler_ = nullptr;

    /// See GCEHandlerExtent
    const GCEHandlerExtent extent_ : 1;

    /// Whether gcode_exceptions().throw_at(this) has been called
    bool is_being_thrown_at_ : 1 = false;
};

/// Provides a "try-catch" block for the "gcode exception system".
/// Only to be used from on marlin thread
///
/// For usage example, see gcode_exception_example.cpp
template <typename F>
class GCodeExceptionHandler final : public GCodeExceptionHandlerBase {

public:
    /// @param extent what type of quick stop is the handler callback able to recover from
    /// @param handle_callback callback function that is called upon recovery if gcode_exceptions().throw_at(this) has been called
    /// Note:
    explicit GCodeExceptionHandler(GCEHandlerExtent extent, F &&handle_callback)
        : GCodeExceptionHandlerBase(extent)
        , handle_callback_(handle_callback) {
    }

    inline ~GCodeExceptionHandler() {
        if (finalize()) {
            handle_callback_();
        }
    }

private:
    F handle_callback_;
};

inline GCodeExceptionHandlerBase **GCodeExceptionManager::next_handler(GCodeExceptionHandlerBase *i) {
    return &(i->next_handler_);
}
