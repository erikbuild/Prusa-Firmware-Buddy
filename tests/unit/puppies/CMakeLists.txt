add_subdirectory(cyphal_node_stubs)
add_subdirectory(minimal_cyphal_node)

add_executable(
  time_sync ${CMAKE_CURRENT_SOURCE_DIR}/time_sync.cpp ${CMAKE_SOURCE_DIR}/src/puppies/time_sync.cpp
  )

target_include_directories(
  time_sync PUBLIC ${CMAKE_SOURCE_DIR}/tests/stubs ${CMAKE_SOURCE_DIR}/include
                   ${CMAKE_SOURCE_DIR}/src/common
  )

target_link_libraries(time_sync SG14 buddy_utils)

add_catch_test(time_sync)
