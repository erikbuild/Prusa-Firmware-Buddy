set(CMAKE_CXX_FLAGS_DEBUG "-O0")
set(CMAKE_C_FLAGS_DEBUG "-O0")

set(DATA_FORMAT_DIR "${PROJECT_ROOT_DIR}/lib/Prusa-Material-Data-Format")

file(GLOB_RECURSE ANFC_PY_DEPS CONFIGURE_DEPENDS "${DATA_FORMAT_DIR}/utils/*"
     "${DATA_FORMAT_DIR}/data/*"
     )

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/autogen/sample_data.cpp.in
         ${CMAKE_CURRENT_BINARY_DIR}/autogen/field_enums.hpp
  DEPENDS gen_anfc_tests_support_files.py ${ANFC_PY_DEPS}
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/gen_anfc_tests_support_files.py
          ${DATA_FORMAT_DIR}
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/autogen
  )

add_executable(
  prusa_nfc_reader_tests
  ${CMAKE_CURRENT_BINARY_DIR}/autogen/field_enums.hpp
  ${CMAKE_CURRENT_BINARY_DIR}/autogen/sample_data.cpp.in prusa_nfc_reader_tests.cpp
  )
target_include_directories(
  prusa_nfc_reader_tests PRIVATE include ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/common
                                 ${CMAKE_SOURCE_DIR}/src/puppy/anfc ${CMAKE_CURRENT_BINARY_DIR}
  )
target_link_libraries(prusa_nfc_reader_tests SG14 prusa_nfc_reader magic_enum buddy_utils)
add_catch_test(prusa_nfc_reader_tests)
