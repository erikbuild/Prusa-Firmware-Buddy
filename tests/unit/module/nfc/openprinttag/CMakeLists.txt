set(CMAKE_CXX_FLAGS_DEBUG "-O0")
set(CMAKE_C_FLAGS_DEBUG "-O0")

set(OPT_DIR "${PROJECT_ROOT_DIR}/lib/OpenPrintTag")

file(GLOB_RECURSE OPT_DEPS CONFIGURE_DEPENDS "${OPT_DIR}/utils/*" "${OPT_DIR}/data/*")

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/autogen/sample_data.cpp.in
         ${CMAKE_CURRENT_BINARY_DIR}/autogen/field_enums.hpp
  DEPENDS gen_opt_tests_support_files.py config_wrong_mime.yaml config_wrong_mime2.yaml ${OPT_DEPS}
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/gen_opt_tests_support_files.py
          ${OPT_DIR}
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/autogen
  )

add_executable(
  openprinttag_reader_tests
  ${CMAKE_CURRENT_BINARY_DIR}/autogen/field_enums.hpp
  ${CMAKE_CURRENT_BINARY_DIR}/autogen/sample_data.cpp.in opt_reader_tests.cpp
  )
target_include_directories(
  openprinttag_reader_tests PRIVATE include ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/common
                                    ${CMAKE_SOURCE_DIR}/src/puppy/anfc ${CMAKE_CURRENT_BINARY_DIR}
  )
target_link_libraries(openprinttag_reader_tests SG14 openprinttag openprinttag_fields buddy_utils)
add_catch_test(openprinttag_reader_tests)
