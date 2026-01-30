target_sources(
  Marlin PRIVATE heatbreak_regulator.cpp heater_watch.cpp marlin_temptable.cpp
                 steady_state_hotend.cpp thermal_model_protection.cpp thermal_runaway.cpp
  )

add_subdirectory(hotend_regulator)
