add_executable(cobs_tests cobs_testing.cpp)

target_link_libraries(cobs_tests cobs SG14)
add_catch_test(cobs_tests)
