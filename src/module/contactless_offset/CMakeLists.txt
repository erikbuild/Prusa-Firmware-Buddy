if(HAS_REMOTE_TOOL_OFFSET_SENSOR)
  add_library(contactless_offset STATIC src/contactless_offset/can_remote_sensor.cpp)
  target_include_directories(contactless_offset PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
  target_link_libraries(
    contactless_offset
    PUBLIC buddy_utils
    PRIVATE dsdl_prusa3d
    )
else()
  add_library(contactless_offset INTERFACE)
  target_include_directories(contactless_offset INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
  target_link_libraries(contactless_offset INTERFACE buddy_utils)
endif()
