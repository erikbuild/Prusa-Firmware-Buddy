# Library that autogenerates enums based on YAMLS in the openprinttag repository

add_library(openprinttag_fields STATIC)

set(OPT_DIR "${PROJECT_ROOT_DIR}/lib/OpenPrintTag")
set(GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/autogen_include/openprinttag/autogen")

file(GLOB_RECURSE OPT_DEPS CONFIGURE_DEPENDS "${OPT_DIR}/utils/*" "${OPT_DIR}/data/*")

add_custom_command(
  OUTPUT "${GEN_DIR}/fields.hpp.in"
  DEPENDS opt_fields_gen.py ${OPT_DEPS}
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/opt_fields_gen.py" ${OPT_DIR}
  WORKING_DIRECTORY "${GEN_DIR}"
  )

# Necessary for triggering recompilation when the generated header updates
target_sources(openprinttag_fields PRIVATE ${GEN_DIR}/fields.hpp.in)

target_include_directories(
  openprinttag_fields PUBLIC include "${CMAKE_CURRENT_BINARY_DIR}/autogen_include"
  )

target_link_libraries(openprinttag_fields PUBLIC openprinttag)
