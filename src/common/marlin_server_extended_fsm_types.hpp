#pragma once

/// @brief Parent of all extended FSM data
class FSMExtendedData {
public:
    FSMExtendedData() = default;

    constexpr bool operator==(const FSMExtendedData &) const = default;
};

/// concept used to check if item is subclass of FSMExtendedData
template <class T>
concept FSMExtendedDataSubclass = std::is_base_of<FSMExtendedData, T>::value;
