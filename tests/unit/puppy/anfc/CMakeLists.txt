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
  ${CMAKE_SOURCE_DIR}/src/puppy/anfc/nfc_prusa/nanocbor_ext.cpp
  ${CMAKE_SOURCE_DIR}/src/puppy/anfc/nfc_prusa/ndef.cpp
  ${CMAKE_SOURCE_DIR}/src/puppy/anfc/nfc_prusa/prusa_nfc_reader.cpp
  prusa_nfc_reader_tests.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/autogen/sample_data.cpp.in
  ${CMAKE_CURRENT_BINARY_DIR}/autogen/field_enums.hpp
  )
target_include_directories(
  prusa_nfc_reader_tests PRIVATE include ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/common
                                 ${CMAKE_SOURCE_DIR}/src/puppy/anfc ${CMAKE_CURRENT_BINARY_DIR}
  )
target_link_libraries(prusa_nfc_reader_tests SG14 nanocbor magic_enum buddy_utils)
add_catch_test(prusa_nfc_reader_tests)
