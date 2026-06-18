/// @file
#pragma once

#include <marlin_server_types/client_response.hpp>
#include <option/has_side_fsensor.h>
#include <option/has_nozzle_cleaner.h>
#include <option/has_loadcell.h>
#include <option/has_mmu2.h>
#include <option/has_extruder_fsensor.h>
#include <option/has_anfc.h>

// define enum classes for responses here
// and YES phase can have 0 responses
// every enum must have "_last"
// EVERY response shall have a unique ID (so every button in GUI is unique)
enum class PhasesLoadUnload : PhaseUnderlyingType {
    initial,
    ChangingTool,
    Parking_stoppable,
    Parking_unstoppable,
    WaitingTemp_stoppable,
    WaitingTemp_unstoppable,
    Ramming_stoppable,
    Ramming_unstoppable,
    Unloading_stoppable,
    Unloading_unstoppable,
    IsFilamentUnloaded,
    FilamentNotInFS,
    ManualUnload_continuable,
    ManualUnload_uncontinuable,
    UserPush_stoppable,
    UserPush_unstoppable,
    MakeSureInserted_stoppable,
    MakeSureInserted_unstoppable,
    Inserting_stoppable,
    Inserting_unstoppable,
    IsFilamentInGear,
    Ejecting_stoppable,
    Ejecting_unstoppable,
#if HAS_SIDE_FSENSOR() && HAS_EXTRUDER_FSENSOR()
    LoadingObstruction_stoppable,
    LoadingObstruction_unstoppable,
#endif
    Loading_stoppable,
    Loading_unstoppable,
    LoadingToGears_stoppable,
    LoadingToGears_unstoppable,
    Purging_stoppable,
    Purging_unstoppable,
    AwaitingFilament_stoppable,
    AwaitingFilament_unstoppable,
    IsColor,
    IsColorPurge,
    Unparking,
#if HAS_NOZZLE_CLEANER()
    UnloadNozzleCleaning,
    LoadNozzleCleaning,
#endif
#if HAS_LOADCELL() && HAS_EXTRUDER_FSENSOR()
    FilamentStuck,
#endif

#if HAS_AUTO_RETRACT()
    AutoRetracting,
#endif

#if HAS_ANFC()
    /// Warning that some filament usage has not been commited to the tag
    OPT_UncommitedUsage,
#endif

#if HAS_INDX()
    FilamentCalibrationFailed,
#endif

#if HAS_MMU2()
    // MMU-specific dialogs
    LoadFilamentIntoMMU,
    MMUDummyStartNoAttention,
    // internal states of the MMU
    MMU_EngagingIdler,
    MMU_DisengagingIdler,
    MMU_UnloadingToFinda,
    MMU_UnloadingToPulley,
    MMU_FeedingToFinda,
    MMU_FeedingToBondtech,
    MMU_FeedingToNozzle,
    MMU_AvoidingGrind,
    MMU_FinishingMoves,
    MMU_ERRDisengagingIdler,
    MMU_ERREngagingIdler,
    MMU_ERRWaitingForUser,
    MMU_ERRInternal,
    MMU_ERRHelpingFilament,
    MMU_ERRTMCFailed,
    MMU_UnloadingFilament,
    MMU_LoadingFilament,
    MMU_SelectingFilamentSlot,
    MMU_PreparingBlade,
    MMU_PushingFilament,
    MMU_PerformingCut,
    MMU_ReturningSelector,
    MMU_ParkingSelector,
    MMU_EjectingFilament,
    MMU_RetractingFromFinda,
    MMU_Homing,
    MMU_MovingSelector,
    MMU_FeedingToFSensor,
    MMU_HWTestBegin,
    MMU_HWTestIdler,
    MMU_HWTestSelector,
    MMU_HWTestPulley,
    MMU_HWTestCleanup,
    MMU_HWTestExec,
    MMU_HWTestDisplay,
    MMU_ErrHwTestFailed,
#endif

    _cnt,
    _last = _cnt - 1
};
constexpr inline ClientFSM client_fsm_from_phase(PhasesLoadUnload) { return ClientFSM::Load_unload; }

namespace ClientResponses {

extern constinit const EnumArray<PhasesLoadUnload, PhaseResponses, CountPhases<PhasesLoadUnload>()> LoadUnloadResponses;

} // namespace ClientResponses
