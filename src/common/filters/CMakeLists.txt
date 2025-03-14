if(BOARD_IS_MASTER_BOARD OR BOARD STREQUAL "DWARF")
  target_sources(firmware PRIVATE median_filter.cpp)
endif()

if(BOARD_IS_MASTER_BOARD)
  target_sources(firmware PRIVATE kalman.cpp)
endif()
