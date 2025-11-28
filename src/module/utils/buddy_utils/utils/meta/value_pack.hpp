/// @file
/// See value_pack_tests.cpp for tests and examples
#pragma once

#include <cstddef>

#include <utils/meta/meta_utils.hpp>
#include <utils/overloaded_visitor.hpp>

template <auto... values_>
struct ValuePack;

template <typename T>
constexpr bool is_value_pack = false;

template <auto... values_>
constexpr bool is_value_pack<ValuePack<values_...>> = true;

template <typename T>
concept CValuePack = is_value_pack<T>;

/// Provides a compile-time array/list like thing where each item can be of any type
/// sizeof() == 0
/// All operations are executed at compile time
template <auto... values_>
struct ValuePack {
    static constexpr size_t size = sizeof...(values_);

public: //* Function-only things
    /// @returns whether @p value is in the pack
    template <typename T>
    static consteval bool contains([[maybe_unused]] T value) {
        [[maybe_unused]] static constexpr auto equals = [](auto a, auto b) {
            if constexpr (requires() { a == b; }) {
                return (a == b);
            } else {
                return false;
            }
        };
        return (equals(value, values_) || ...);
    }

    /// @returns whether two ValuePacks equal
    template <auto... values2>
    static consteval bool equals(ValuePack<values2...>) {
        return std::is_same_v<ValuePack<values_...>, ValuePack<values2...>>;
    }

    /// @returns whether two ValuePacks equal
    template <typename T>
    static consteval bool equals() {
        return equals(T {});
    }

    consteval bool operator==(const CValuePack auto &o) const {
        return equals(o);
    }
    consteval bool operator!=(const CValuePack auto &o) const {
        return !equals(o);
    }

public: //* Static functions
    /// @returns flattened value pack - nested ValuePacks are expanded
    template <typename = void>
    static consteval auto flatten() {
        static constexpr auto f = []<auto... av, auto b>(ValuePack<av...> a, ValuePack<b>) {
            if constexpr (is_value_pack<decltype(b)>) {
                return a.concatenate(b.flatten());
            } else {
                return a.template append<b>();
            }
        };
        return accumulate_arguments(f, ValuePack<values_> {}...);
    };

    /// @returns value pack with duplicates remove
    /// The pack does not have to be sorted
    template <typename = void>
    static consteval auto unique() {
        static constexpr auto f = []<auto... av, auto b>(ValuePack<av...> a, ValuePack<b>) {
            if constexpr (a.contains(b)) {
                return a;
            } else {
                return a.template append<b>();
            }
        };
        return accumulate_arguments(f, ValuePack<values_> {}...);
    }

    /// Appends one item to the value pack
    /// @returns value pack instance with the item appended
    template <auto... items>
    static consteval auto append() {
        return ValuePack<values_..., items...> {};
    }

    /// Contatenates two value packs
    /// @returns concatenated value pack instance
    template <auto... other_values>
    static consteval auto concatenate(ValuePack<other_values...>) {
        return ValuePack<values_..., other_values...> {};
    }

public: //* Type-only things
        /// Passes the ValuePack values to the provided @p Template
    /// @returns Template<values...>
    template <template <auto...> typename Template>
    using ApplyOn = Template<values_...>;

public: //* Type "wrappers" for functions
        // All functions are static (because the sizeof the pack is actually zero and all the data is in the template pack)
        // These are convenience "functions" that return the resulting type directly
        // So that you can do `ValuePack<1>::Op<ValuePack<2>>` instead of `decltype(ValuePack<1>::op(ValuePack<2>{}))`
    template <typename T = void>
    using Flatten = decltype(flatten<T>());

    template <typename T = void>
    using Unique = decltype(unique<T>());

    template <auto... items>
    using Append = decltype(append<items...>());

    template <CValuePack T>
    using Concatenate = decltype(concatenate(T {}));
};
