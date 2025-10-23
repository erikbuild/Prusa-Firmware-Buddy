/// @file
#pragma once

#include <cstdint>

// !!! DO NOT REORDER, DO NOT CHANGE - this is used in config store
enum class SoundMode : uint8_t {
    once,
    loud,
    silent,
    assist,
    _count,
    _last = _count - 1,
    _undef = 0xFF,
    _default_sound = loud,
};

enum class SoundType : uint8_t {
    button_echo,
    standard_prompt,
    standard_alert,
    critical_alert,
    encoder_move,
    blind_alert,
    start,
    single_beep,
    waiting_beep,
    single_beep_always_loud, // Single beep that ignores SoundMode settings, so we can play the sound in self tests
};
