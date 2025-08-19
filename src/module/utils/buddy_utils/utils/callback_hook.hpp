/// \file
#pragma once

#include <inplace_function.hpp>

template <typename... Args>
class CallbackHookGuard;

/// Point for registering callbacks to
/// Represented as a linked list
/// !!! Not thread-safe
template <typename... Args>
class CallbackHookPoint {

public:
    using Item = CallbackHookGuard<Args...>;
    friend Item;

public:
    /// Calls all registered hooks
    /// The execution order depends on the insertion order - newer hooks execute first.
    void call_all(Args &&...args) {
        for (auto it = first_; it; it = it->next_) {
            it->callback_(std::forward<Args>(args)...);
        }
    }

private:
    void insert(Item *item) {
        item->next_ = first_;
        first_ = item;
    }

    void remove(Item *item) {
        Item **current = &first_;
        while (*current != item) {
            assert(*current);
            current = &((*current)->next_);
        }
        *current = (*current)->next_;
    }

private:
    Item *first_ = nullptr;
};

/// Guard that registers the provided callback to the specified point
/// The hook gets removed when the function is destroyed
/// !!! Not thread safe
template <typename... Args>
class CallbackHookGuard {
    friend class CallbackHookPoint<Args...>;

public:
    using Callback = stdext::inplace_function<void(Args...)>;
    using Point = CallbackHookPoint<Args...>;

public:
    // Note: Template deducation problems without the "auto"
    CallbackHookGuard(CallbackHookPoint<Args...> &point, const auto &cb)
        : point_(point)
        , callback_(cb) {
        point_.insert(this);
    }

    ~CallbackHookGuard() {
        point_.remove(this);
    }

private:
    Point &point_;
    CallbackHookGuard *next_ = nullptr;
    Callback callback_;
};
