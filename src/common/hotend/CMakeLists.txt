target_sources(firmware PRIVATE hotend.cpp)

target_sources(firmware PRIVATE hotend/base_hotend.cpp)

if(${BOARD} STREQUAL "BUDDY")
  target_sources(firmware PRIVATE hotend/local_hotend.cpp printer/hotends_buddy.cpp)

elseif(${BOARD} STREQUAL "XBUDDY")
  target_sources(firmware PRIVATE hotend/local_hotend.cpp printer/hotends_xbuddy.cpp)

elseif(${BOARD} STREQUAL "XLBUDDY")
  target_sources(
    firmware PRIVATE hotend/dummy_hotend.cpp hotend/dwarf_hotend.cpp printer/hotends_xlbuddy.cpp
    )

elseif(${BOARD} STREQUAL "DWARF")
  target_sources(firmware PRIVATE hotend/local_hotend.cpp printer/hotends_dwarf.cpp)

else()
  message(FATAL_ERROR Unknown hotends configuration)
endif()
