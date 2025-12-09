# buddy_openprinttag_multirequest_tests
set(OPT_SRC "${CMAKE_SOURCE_DIR}/src/common/feature/openprinttag")

add_executable(
  buddy_openprinttag_multirequest_tests
  ${OPT_SRC}/detail/requests_base.cpp ${OPT_SRC}/detail/requests_multi_base.cpp
  buddy_openprinttag_multirequest_tests.cpp
  )
target_include_directories(
  buddy_openprinttag_multirequest_tests PRIVATE stub_include ${CMAKE_SOURCE_DIR}/src/common
  )
target_link_libraries(buddy_openprinttag_multirequest_tests openprinttag_fields buddy_utils)
add_catch_test(buddy_openprinttag_multirequest_tests)
