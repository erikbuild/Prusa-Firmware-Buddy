# cobs_tests
# ==========================
add_executable(cobs_tests cobs_testing.cpp)
target_link_libraries(cobs_tests cobs SG14)
add_catch_test(cobs_tests)

# base64_tests
# ==========================
add_executable(base64_tests base64_tests.cpp)
target_link_libraries(base64_tests base64)
add_catch_test(base64_tests)
