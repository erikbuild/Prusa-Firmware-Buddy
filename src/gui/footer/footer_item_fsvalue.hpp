#pragma once
#include "ifooter_item.hpp"
#include <option/has_side_fsensor.h>

class FooterItemFSValue : public FooterIconText_IntVal {
    static string_view_utf8 static_makeView(int value);
    static int static_readValue();

public:
    FooterItemFSValue(window_t *parent);
};

#if HAS_SIDE_FSENSOR()
    #include <atomic>

class IFSensor;

class FooterItemFSValueSide : public FooterIconText_IntVal {
    static string_view_utf8 static_makeView(int value);
    static int static_readValue();

    /// Override target. Used during fsensor selftest so the footer reflects the
    /// tool being calibrated rather than the currently-picked tool.
    static inline std::atomic<IFSensor *> selftest_override_ { nullptr };

public:
    FooterItemFSValueSide(window_t *parent);

    /// Pin the footer value to \p sensor, ignoring the currently-picked tool.
    /// Pass nullptr to clear.
    static void set_selftest_override(IFSensor *sensor) { selftest_override_.store(sensor); }
};
#endif
