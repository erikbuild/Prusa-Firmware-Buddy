target_include_directories(firmware PRIVATE .)

target_sources(
  firmware PRIVATE can_driver_fdcan.cpp cyphal_record.cpp cyphal_simple_register.cpp
                   cyphal_task.cpp cyphal_timesync.cpp
  )
