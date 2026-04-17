target_sources(marlin_server_types PUBLIC filament_change_phases.cpp safety_timer_phases.cpp)

if(HAS_INDX)
  target_sources(marlin_server_types PUBLIC nozzle_mismatch_phases.cpp)
endif()
