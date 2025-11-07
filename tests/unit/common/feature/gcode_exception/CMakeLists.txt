add_executable(
  gcode_exception_tests
  ${CMAKE_SOURCE_DIR}/src/common/feature/gcode_exception/gcode_exception.cpp
  ${CMAKE_SOURCE_DIR}/src/common/feature/gcode_exception/gcode_exception_instance.cpp
  gcode_exception_tests.cpp
  )
target_link_libraries(gcode_exception_tests buddy_utils)
target_include_directories(gcode_exception_tests PRIVATE stub ${CMAKE_SOURCE_DIR}/src/common)
add_catch_test(gcode_exception_tests)
