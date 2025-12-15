#include "tool_index.hpp"
#include <printers.h>
#include <module/motion.h>
#include <utils/overloaded_visitor.hpp>

#include <option/has_tool_mapping.h>
#if HAS_TOOL_MAPPING()
    #include <module/prusa/tool_mapper.hpp>
#endif

#include <option/has_toolchanger.h>
#if HAS_TOOLCHANGER()
    #include <module/prusa/toolchanger.h>
#endif

#include <option/has_mmu2.h>
#if HAS_MMU2()
    #include <feature/prusa/MMU2/mmu2_mk4.h>
#endif

#include <option/board_is_master_board.h>

using VirtualExtension = VirtualToolIndexExtension<VirtualToolIndex>;
using PhysicalExtension = PhysicalToolIndexExtension<PhysicalToolIndex>;
using GcodeExtension = GcodeToolIndexExtension<GcodeToolIndex>;

template <>
PhysicalToolIndex VirtualExtension::to_physical() const {
    [[maybe_unused]] const auto &self = static_cast<const VirtualToolIndex &>(*this);

#if HAS_TOOLCHANGER()
    static_assert(PhysicalToolIndex::count == VirtualToolIndex::count);
    return PhysicalToolIndex::from_raw(self.to_raw());
#else
    static_assert(PhysicalToolIndex::count == 1);
    return PhysicalToolIndex::from_raw(0);
#endif
}

template <>
std::variant<VirtualToolIndex, NoTool> GcodeExtension::to_virtual() const {
    [[maybe_unused]] const auto &self = static_cast<const GcodeToolIndex &>(*this);

#if HAS_TOOL_MAPPING()
    return tool_mapper.to_virtual(self);
#else
    return VirtualToolIndex::from_raw(self.to_raw());
#endif
}

template <>
std::variant<PhysicalToolIndex, NoTool> GcodeExtension::to_physical() const {
    using Result = std::variant<PhysicalToolIndex, NoTool>;
    return match(
        to_virtual(), //
        [](VirtualToolIndex t) -> Result { return t.to_physical(); }, //
        [](NoTool) -> Result { return NoTool {}; });
}

template <>
bool PhysicalExtension::is_enabled() const {
    [[maybe_unused]] const auto &self = static_cast<const PhysicalToolIndex &>(*this);

#if HAS_TOOLCHANGER()
    return prusa_toolchanger.is_tool_enabled(self.to_raw());
#else
    static_assert(PhysicalToolIndex::count == 1);
    return true;
#endif
}

template <>
bool VirtualExtension::is_enabled() const {
    [[maybe_unused]] const auto &self = static_cast<const VirtualToolIndex &>(*this);

#if HAS_TOOLCHANGER()
    return self.to_physical().is_enabled();

#elif HAS_MMU2()
    static_assert(PhysicalToolIndex::count == 1);
    if (MMU2::mmu2.Enabled()) {
        // MMU has five slots, the virtual tool count should match
        static_assert(VirtualToolIndex::count == 5);

        // MMU is enabled - we have access to all virtual tools and can toolchange between them
        return true;

    } else {
        // Without the MMU, we only have one virtual tool available (there's nothing that would do the toolchange)
        return self.to_raw() == 0;
    }
#else
    // Single nozzle, single slot - nothing to get disabled
    static_assert(PhysicalToolIndex::count == 1);
    static_assert(VirtualToolIndex::count == 1);
    return true;
#endif
}

template <>
bool GcodeExtension::is_enabled() const {
    const auto &self = static_cast<const GcodeToolIndex &>(*this);

    return match(
        self.to_virtual(), //
        [](VirtualToolIndex t) { return t.is_enabled(); }, //
        [](NoTool) { return false; });
}

template <>
std::variant<PhysicalToolIndex, NoTool> PhysicalExtension::currently_selected() {
#if HAS_TOOLCHANGER()
    static_assert(!HAS_MMU2());
    static_assert(PhysicalToolIndex::count == PrusaToolChanger::MARLIN_NO_TOOL_PICKED);
    if (active_extruder == PrusaToolChanger::MARLIN_NO_TOOL_PICKED) {
        return NoTool {};
    } else {
        // Toolchanger uses active_extruder for determining tools
        static_assert(PhysicalToolIndex::count == VirtualToolIndex::count);
        return PhysicalToolIndex::from_raw(active_extruder);
    }
#else
    // Single tool, no toolchanber, cannot unpick it
    static_assert(PhysicalToolIndex::count == 1);
    return PhysicalToolIndex::from_raw(0);
#endif
}

template <>
std::variant<VirtualToolIndex, NoTool> VirtualExtension::currently_selected() {
#if HAS_TOOLCHANGER()
    static_assert(!HAS_MMU2());
    static_assert(PhysicalToolIndex::count == PrusaToolChanger::MARLIN_NO_TOOL_PICKED);
    if (active_extruder == PrusaToolChanger::MARLIN_NO_TOOL_PICKED) {
        return NoTool {};
    } else {
        return VirtualToolIndex::from_raw(active_extruder);
    }

#elif HAS_MMU2()
    static_assert(PhysicalToolIndex::count == 1);
    if (MMU2::mmu2.Enabled()) {
        const auto e = MMU2::mmu2.get_current_tool();
        if (e == MMU2::FILAMENT_UNKNOWN) {
            return NoTool {};
        } else {
            return VirtualToolIndex::from_raw(e);
        }
    } else {
        return VirtualToolIndex::from_raw(0);
    }

#else
    static_assert(VirtualToolIndex::count == 1);
    return VirtualToolIndex::from_raw(0);
#endif
}

template <>
std::variant<PhysicalToolIndex, NoTool> PhysicalExtension::from_raw_notool(uint8_t index) {
#if PRINTER_IS_PRUSA_XL() && BOARD_IS_MASTER_BOARD()
    // count on XL is set to EXTRUDERS - 1 / HOTENDS - 1,
    // because of legacy definition of EXTRUDERS / HOTENDS to 6 instead of 5.
    // Values 0 to 4 are actual tools, 5 represents no tool (using MARLIN_NO_TOOL_PICKED).
    // If we encounter MARLIN_NO_TOOL_PICKED we need to return NoTool and check for it on call site.
    static_assert(PhysicalToolIndex::count == PrusaToolChanger::MARLIN_NO_TOOL_PICKED);
    if (index == PrusaToolChanger::MARLIN_NO_TOOL_PICKED) {
        return NoTool {};
    }
#endif
    // Other non-tool values treat as invalid values -> raise bsod
    return PhysicalToolIndex::from_raw(index);
}

template <>
std::variant<VirtualToolIndex, NoTool> VirtualExtension::from_raw_notool(uint8_t index) {
#if HAS_TOOL_MAPPING()
    #if PRINTER_IS_PRUSA_XL() && HAS_TOOLCHANGER()
    // count on XL is set to EXTRUDERS - 1 / HOTENDS - 1,
    // because of legacy definition of EXTRUDERS / HOTENDS to 6 instead of 5.
    // Values 0 to 4 are actual tools, 5 represents no tool (using MARLIN_NO_TOOL_PICKED).
    // If we encounter MARLIN_NO_TOOL_PICKED we need to return NoTool and check for it on call site.
    static_assert(VirtualToolIndex::count == PrusaToolChanger::MARLIN_NO_TOOL_PICKED);
    if (index == PrusaToolChanger::MARLIN_NO_TOOL_PICKED) {
        return NoTool {};
    }
    #endif

    // There is 255 (NO_TOOL_MAPPED) used as no tool at some places.
    if (index == ToolMapper::NO_TOOL_MAPPED) {
        return NoTool {};
    }
#endif
    // Other non-tool values treat as invalid values -> raise bsod
    return VirtualToolIndex::from_raw(index);
}

#if BOARD_IS_MASTER_BOARD()

template <>
string_view_utf8 PhysicalToolIndex::display_name(DisplayNameParams &params) {
    return _("Tool %i").formatted(params, display_index());
}

template <>
string_view_utf8 VirtualToolIndex::display_name(DisplayNameParams &params) {
    if constexpr (PhysicalToolIndex::count == VirtualToolIndex::count) {
        // For multi-tool printers with 1:1 mapping between virtual and physica tools, introducing some extra slots/indexing would just confuse the users.
        // So in that case, refer to the virtual tools same as to the physical ones, because they're are 1:1.
        return to_physical().display_name(params);
    } else {
        return _("Filament Slot %i").formatted(params, display_index());
    }
}

template <>
string_view_utf8 GcodeToolIndex::display_name(DisplayNameParams &params) {
    return _("GCode Tool %i").formatted(params, display_index());
}

#endif
