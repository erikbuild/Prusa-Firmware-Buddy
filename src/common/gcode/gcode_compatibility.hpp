/// @file
#pragma once

#include <optional>

#include <common/hw_check.hpp>
#include <utils/storage/enum_bitset.hpp>
#include <utils/enum_array.hpp>
#include <tool_index.hpp>
#include <inplace_function.hpp>
#include <string_view_utf8.hpp>

#include <option/has_gcode_compatibility.h>
#include <option/has_mmu2.h>

#include <option/has_tool_mapping.h>
#if HAS_TOOL_MAPPING()
// Do not include the header, prevent dependency hell
class ToolMapper;
#endif

#include <option/has_spool_join.h>
#if HAS_SPOOL_JOIN()
// Do not include the header, prevent dependency hell
class SpoolJoin;
#endif

// Do not include the header, prevent dependency hell
class GCodeInfo;

namespace buddy::gcode_compatibility {

struct CheckMetadata {
    /// Severity if the check fails
    /// Can either be a hardcoded severity or a HWCheckType with user-configurable severity
    std::variant<HWCheckSeverity, HWCheckType> severity;

    HWCheckSeverity evaluate_severity() const;

    /// Translatable error message, WITHOUT a trailing '.'
    const char *title;

    /// Translatable long message. Can provide further information. Written in full sentences.
    const char *description = nullptr;
};

enum class GeneralCheck : uint8_t {
    /// Fails if the gcode is not compatible at all, not even in compatibility mode
    /// Checked by M862.2 or M862.3 or printer_model (from comments)
    printer_model,

#if HAS_GCODE_COMPATIBILITY()
    /// Fails if any gcode compatibility mode needs to be applied
    gcode_compatibility_mode,
#endif

    /// I have no idea what this is for
    /// Checked by M862.5
    gcode_level,

    /// Fails if the gcode minimum FW version is higher than ours
    /// Checked by M862.4
    minimum_fw_version,

    /// Fails if the gcode was not sliced with input shaper support
    input_shaper,

#if HAS_MMU2()
    /// Fails if the gcode requires MMU and we don't have it
    mmu,
#endif

    /// The GCode requests some features the printer doesn't have
    unsupported_features,

    _cnt
};

enum class VirtualToolCheck : uint8_t {
    /// The mapped tool needs to be available (and of the right type)
    correct_tool,

    /// Whether the tool has the right nozzle diameter
    /// Checked by M862.1
    nozzle_diameter,

    /// Whether the nozzle is hardened, if the gcode requires it
    /// Checked by M862.1
    nozzle_hardened,

    /// Fails if the gcode is sliced for the HF nozzle and we don't have it
    /// Checked by M862.1
    nozzle_high_flow,

    /// Fails if the nozzle is high flow and the g-code is sliced for a non-HF one
    /// Checked only for the MMU prints.
    /// With MMU:
    /// - Slicing with a non-HF nozzle while HF nozzle is installed results in unsufficient purging.
    /// - Slicing for a HF nozzle without having it leads to extruder skipping.
    /// Checked by M862.1
    nozzle_not_high_flow,

    /// Fails if a filament is not loaded into the tool
    /// This is checked through the filament sensor
    filament_loaded,

    _cnt
};

enum class GCodeToolCheck : uint8_t {
    /// Fails if the gcode tool is not assigned to anything
    tool_assigned,

    _cnt
};

template <typename Check_>
struct ChecksTraits {
    using Check = Check_;

    using Metadata = const EnumArray<Check, CheckMetadata, Check::_cnt>;
    static Metadata metadata;

    using Bitset = EnumBitset<Check, Check::_cnt>;

    /// @returns false if the iteration should stop
    using Visitor = stdext::inplace_function<bool(const CheckMetadata &)>;

    /// @returns false if the iteration stopped by visitor returning false
    static bool visit_set_bits(const Bitset &bitset, const Visitor &visitor) {
        for (uint8_t i = 0; i < std::to_underlying(Check::_cnt); i++) {
            if (bitset.test(i)) {
                if (!visitor(metadata[i])) {
                    return false;
                }
            }
        }

        return true;
    }

    static HWCheckSeverity failure_severity(const Bitset &failed_checks) {
        HWCheckSeverity result = HWCheckSeverity::Ignore;

        visit_set_bits(failed_checks, [&](const CheckMetadata &meta) {
            result = std::max(result, meta.evaluate_severity());
        });

        return result;
    };
};

struct CompatibilityReport {

    template <typename Check_>
    struct FailedChecks {
        using Check = Check_;

        using ChecksMetadata = const EnumArray<Check, CheckMetadata, Check::_cnt>;
        static ChecksMetadata checks_metadata;

        using Bitset = EnumBitset<Check, Check::_cnt>;
        Bitset bitset;

        using Visitor = stdext::inplace_function<void(const CheckMetadata &)>;
        void visit(const Visitor &visitor) const {
            for (uint8_t i = 0; i < std::to_underlying(Check::_cnt); i++) {
                if (bitset.test(i)) {
                    visitor(checks_metadata[i]);
                }
            }
        }

        HWCheckSeverity failure_severity() const {
            HWCheckSeverity result = HWCheckSeverity::Ignore;

            visit([&](const CheckMetadata &meta) {
                result = std::max(result, meta.evaluate_severity());
            });

            return result;
        };
    };

    ChecksTraits<GeneralCheck>::Bitset failed_general_checks;
    StrongIndexArray<ChecksTraits<VirtualToolCheck>::Bitset, VirtualToolIndex::count, VirtualToolIndex, VirtualToolIndex::to_raw_static> failed_virtual_tool_checks;
    StrongIndexArray<ChecksTraits<GCodeToolCheck>::Bitset, GcodeToolIndex::count, GcodeToolIndex, GcodeToolIndex::to_raw_static> failed_gcode_tool_checks;

    struct FailedCheck {
        using Tool = std::variant<VirtualToolIndex, GcodeToolIndex, NoTool>;

        const CheckMetadata *meta;
        Tool tool;
    };

    /// @returns false if the iteration should stop
    using FailedCheckVisitor = stdext::inplace_function<bool(const FailedCheck &)>;

    /// @returns false if the iteration stopped by visitor returning false
    bool visit_failed_checks(const FailedCheckVisitor &visitor) const;

    /// @returns (first) failed check of the highest severity
    /// @param check_filter if provided, only considers check for the specified tool
    std::optional<FailedCheck> highest_severity_failed_check(std::optional<FailedCheck::Tool> check_filter = std::nullopt) const;

    /// Severity of the failures
    HWCheckSeverity failure_severity() const;

    static const GCodeInfo &default_gcode_info();
#if HAS_TOOL_MAPPING()
    static const ToolMapper &default_tool_mapper();
#endif
#if HAS_SPOOL_JOIN()
    static const SpoolJoin &default_spool_join();
#endif

    /// Checks compatibility of GCodeInfo against the current printer state.
    /// Stores the result in the CompatibilityReport itself.
    /// Does not report things that might be affected by toolmapping
    void generate_without_toolmapping(const GCodeInfo &gcode_info = default_gcode_info());

    struct ToolMappingArgs {
        const GCodeInfo &gcode_info = default_gcode_info();

#if HAS_TOOL_MAPPING()
        const ToolMapper &tool_mapper = default_tool_mapper();
#endif
#if HAS_SPOOL_JOIN()
        const SpoolJoin &spool_join = default_spool_join();
#endif
    };

    /// Checks compatibility of GCodeInfo against the current printer and tool mapping state.
    /// Stores the result in the CompatibilityReport itself.
    /// Only considers things that might be affected by toolmapping.
    void generate_toolmapping_only(const ToolMappingArgs &args);

    /// generate_without_toolmapping + generate_toolmapping_only
    void generate_full(const ToolMappingArgs &args);

    /// If there is a failed check with abort severity, shows that one.
    /// Otherwise shows a warning for each failed check with the warning severity.
    /// The user needs to confirm ignoring all of the warnings.
    /// Some warning ignores can change printer state (for example filament not loaded disables FS)
    /// @returns true if the user confirmed to skip all warnings
    /// !!! TO BE EXECUTED FROM THE GUI THREAD ONLY
    [[nodiscard]] bool gui_confirm_all_incompatibilities() const;

private:
    void generate_toolmapping_only_noclear(const ToolMappingArgs &args);
};

} // namespace buddy::gcode_compatibility
