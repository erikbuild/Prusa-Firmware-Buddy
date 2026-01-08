set(OPT_SRC "${CMAKE_SOURCE_DIR}/src/common/feature/openprinttag")

# buddy_openprinttag_multirequest_tests
add_executable(
  buddy_openprinttag_multirequest_tests
  ${OPT_SRC}/detail/requests_base.cpp ${OPT_SRC}/detail/requests_multi_base.cpp
  ${OPT_SRC}/tool_tag.cpp buddy_openprinttag_multirequest_tests.cpp
  )
target_include_directories(
  buddy_openprinttag_multirequest_tests PRIVATE stub_include/tool_tag
                                                ${CMAKE_SOURCE_DIR}/src/common
  )
target_link_libraries(
  buddy_openprinttag_multirequest_tests openprinttag_fields buddy_utils logging-mock
  )
add_catch_test(buddy_openprinttag_multirequest_tests)

# buddy_openprinttag_datautils_tests
add_executable(
  buddy_openprinttag_datautils_tests
  ${CMAKE_SOURCE_DIR}/src/common/filament_common.cpp
  ${CMAKE_SOURCE_DIR}/src/common/filament_list.cpp
  ${CMAKE_SOURCE_DIR}/src/common/filament_presets.cpp
  ${OPT_SRC}/data_utils.cpp
  ${OPT_SRC}/detail/requests_base.cpp
  ${OPT_SRC}/detail/requests_multi_base.cpp
  ${OPT_SRC}/tool_tag.cpp
  buddy_openprinttag_datautils_tests.cpp
  )
target_include_directories(
  buddy_openprinttag_datautils_tests
  PRIVATE stub_include/data_utils stub_include/tool_tag stub_include/requests
          ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src/common
  )
target_link_libraries(
  buddy_openprinttag_datautils_tests openprinttag_fields buddy_utils logging-mock
  )
add_catch_test(buddy_openprinttag_datautils_tests)
