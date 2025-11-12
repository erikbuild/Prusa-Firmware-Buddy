#pragma once

#include <concepts>
#include <cassert>

/// Class representing an intrusive linked list
/// @param Item item type. The linked list doesn't own or copy the items
/// @param next_ref_f Function that returns pointer to the next item for a given item
template <typename Item, auto next_ref_f>
class SingleLinkedList {
    static_assert(std::is_invocable_r_v<Item **, decltype(next_ref_f), Item *>);

public:
    inline bool empty() const {
        return front_ == nullptr;
    }

    inline Item *front() {
        return front_;
    }

    /// Adds the item to the front of the linked list
    /// !!! Item is not copied, the referenced instance itself is linked in to the list
    void push_front(Item *item) {
        assert(item);
        *next_ref_f(item) = front_;
        front_ = item;
    }

    void pop_front() {
        assert(front_);
        front_ = *next_ref_f(front_);
    }

    bool remove(Item *item) {
        Item **ii = &front_;
        while (Item *i = *ii) {
            if (i == item) {
                *ii = *next_ref_f(i);
                return true;
            }

            ii = next_ref_f(i);
        }

        return false;
    }

public:
    struct iterator {
        friend class SingleLinkedList;

    public:
        using difference_type = std::ptrdiff_t;
        using value_type = Item *;

    public:
        iterator() = default;
        iterator(const iterator &) = default;

        inline iterator &operator++() {
            i_ = *next_ref_f(i_);
            return *this;
        }
        inline iterator &operator++(int) {
            return operator++();
        }

        inline Item *operator*() const {
            return i_;
        }

        bool operator==(const iterator &) const = default;

        iterator &operator=(const iterator &) = default;

    private:
        explicit iterator(Item *i)
            : i_(i) {}

        Item *i_ = nullptr;
    };

    iterator begin() {
        return iterator { front_ };
    }

    iterator end() {
        return iterator { nullptr };
    }

private:
    Item *front_ = nullptr;
};
