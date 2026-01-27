/// @file
#pragma once

#include <span>
#include <atomic>

#include "base_hotend.hpp"

/// Represents a hotend that is controlled on the current processor (not on a dwarf)
class LocalHotend final : public BaseHotend {

public:
    using TempTable = std::span<const short[2]>;

    struct Config {
        BaseHotend::Config base_config;
        /// Temperature table for mapping raw temperature readouts
        TempTable nozzle_temp_table;
    };

public:
    /// !!! Careful, the config pointer is stored, so make sure the config is persistent!
    explicit LocalHotend(PhysicalToolIndex tool, const Config *config);

protected:
    virtual void manage() override;

    virtual void isr_on_readings_ready() override;

protected:
    const Config &local_config_;

    /// Written from the Temperature ISR, read from the defaultTask
    /// !!! Contains a sum of OVERSAMPLENR samples
    std::atomic<uint16_t> nozzle_raw_temp_;
};
