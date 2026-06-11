add_executable(tarball_tests ${CMAKE_SOURCE_DIR}/src/resources/tarball.cpp tarball_tests.cpp)
target_include_directories(
  tarball_tests PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/include
  )
target_link_libraries(tarball_tests logging-mock SG14)
add_catch_test(tarball_tests)
