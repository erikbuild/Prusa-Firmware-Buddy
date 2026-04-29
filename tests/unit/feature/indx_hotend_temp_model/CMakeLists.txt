# indx_hotend_temp_compensation_tests
add_executable(
  indx_hotend_temp_compensation_tests
  ${CMAKE_SOURCE_DIR}/src/feature/indx_hotend_temp_model/indx_hotend_temp_compensation.cpp
  indx_hotend_temp_compensation_tests.cpp
  )
target_include_directories(indx_hotend_temp_compensation_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_catch_test(indx_hotend_temp_compensation_tests)
