/// @file
#pragma once

#include <array>
#include <cstddef>

namespace strong_index_array {

enum class AllowWeakIndexing {
    /// Allow both Index type and size_t to be used for indexing
    yes,
    /// Allow only Index type to be used for indexing
    no
};

}

/// Extends std::array to support custom index types
/// @param Item type of item in array
/// @param Index type used for indexing
/// @param capacity size of array
/// @param index_f conversion function to get raw index from Index type
/// @param weak_indexing specifies if size_t can be used for indexing
template <typename Item, size_t capacity, typename Index, auto index_f, strong_index_array::AllowWeakIndexing weak_indexing = strong_index_array::AllowWeakIndexing::no>
class StrongIndexArray : public std::array<Item, capacity> {

public:
    using BaseArray = std::array<Item, capacity>;

public:
    template <typename... Arg>
    inline constexpr StrongIndexArray(Arg &&...arg)
        : BaseArray { std::forward<Arg>(arg)... } {}

    inline constexpr const Item &operator[](Index index) const {
        return BaseArray::operator[](index_f(index));
    }

    inline constexpr Item &operator[](Index index) {
        return BaseArray::operator[](index_f(index));
    }

    inline constexpr const Item &at(Index index) const {
        return BaseArray::at(index_f(index));
    }

    inline constexpr Item &at(Index index) {
        return BaseArray::at(index_f(index));
    }

    inline constexpr Item &operator[](size_t index)
        requires(weak_indexing == strong_index_array::AllowWeakIndexing::yes)
    {
        return BaseArray::operator[](index);
    }

    inline constexpr const Item &operator[](size_t index) const
        requires(weak_indexing == strong_index_array::AllowWeakIndexing::yes)
    {
        return BaseArray::operator[](index);
    }

    inline constexpr Item &at(size_t index)
        requires(weak_indexing == strong_index_array::AllowWeakIndexing::yes)
    {
        return BaseArray::at(index);
    }

    inline constexpr const Item &at(size_t index) const
        requires(weak_indexing == strong_index_array::AllowWeakIndexing::yes)
    {
        return BaseArray::at(index);
    }
};
