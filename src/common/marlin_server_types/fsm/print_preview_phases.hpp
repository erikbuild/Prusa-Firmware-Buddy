#pragma once
#include <marlin_server_types/client_response.hpp>
#include <printers.h>
#include <utils/enum_array.hpp>
#include <option/has_mmu2.h>
#include <option/has_tool_mapping.h>
#include <option/has_e2ee_support.h>

enum class PhasesPrintPreview : PhaseUnderlyingType {
    loading,
    download_wait,
    main_dialog,
    unfinished_selftest,
    new_firmware_available,
    wrong_printer,
    wrong_printer_abort,
    filament_not_inserted,
#if HAS_MMU2()
    mmu_filament_inserted,
#endif
#if HAS_TOOL_MAPPING()
    tools_mapping,
#endif
    wrong_filament,
#if HAS_E2EE_SUPPORT()
    untrusted_identity,
#endif
    file_error, ///< Something is wrong with the gcode file
    _last = file_error
};

constexpr inline ClientFSM client_fsm_from_phase(PhasesPrintPreview) { return ClientFSM::PrintPreview; }

namespace ClientResponses {

inline constexpr EnumArray<PhasesPrintPreview, PhaseResponses, CountPhases<PhasesPrintPreview>()> PrintPreviewResponses {
    { PhasesPrintPreview::loading, {} },
        { PhasesPrintPreview::download_wait, {
                                                 Response::Quit,
                                             } },
        { PhasesPrintPreview::main_dialog, {
#if PRINTER_IS_PRUSA_XL()
                                               Response::Continue,
#elif PRINTER_IS_PRUSA_MINI()
                                               Response::PRINT,
#else
                                               Response::Print,
#endif
                                               Response::Back,
                                           } },
        { PhasesPrintPreview::unfinished_selftest, {
                                                       Response::Ignore,
                                                       Response::Calibrate,
                                                   } },
        { PhasesPrintPreview::new_firmware_available, {
                                                          Response::Continue,
                                                      } },
        { PhasesPrintPreview::wrong_printer, {
                                                 Response::Abort,
                                                 Response::PRINT,
                                             } },
        { PhasesPrintPreview::wrong_printer_abort, {
                                                       Response::Abort,
                                                   } },
        { PhasesPrintPreview::filament_not_inserted, {
                                                         Response::Yes,
                                                         Response::No,
                                                         Response::FS_disable,
                                                     } },
#if HAS_MMU2()
        { PhasesPrintPreview::mmu_filament_inserted, {
                                                         Response::Yes,
                                                         Response::No,
                                                     } },
#endif
#if HAS_TOOL_MAPPING()
        { PhasesPrintPreview::tools_mapping, {
                                                 Response::Back,
                                                 Response::Filament,
                                                 Response::PRINT,
                                             } },
#endif
        { PhasesPrintPreview::wrong_filament, {
#if !PRINTER_IS_PRUSA_XL()
                                                  Response::Change,
#endif
                                                  Response::Ok,
                                                  Response::Abort,
                                              } },
#if HAS_E2EE_SUPPORT()
        { PhasesPrintPreview::untrusted_identity, { Response::Yes, Response::No, Response::Abort } },
#endif
        { PhasesPrintPreview::file_error, {
                                              Response::Abort,
                                          } },
};

} // namespace ClientResponses
