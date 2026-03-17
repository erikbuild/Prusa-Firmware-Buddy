if(NOT DEFINED NO_CCACHE)
  find_program(CCACHE ccache OPTIONAL)

  if(CCACHE STREQUAL "CCACHE-NOTFOUND")
    message(STATUS "CCache not found")
  else()
    message(STATUS "CCache found: ${CCACHE}")

    set(CMAKE_CXX_COMPILER_LAUNCHER
        "${CCACHE};sloppiness=pch_defines,time_macros;namespace=${PRINTER}-${CMAKE_BUILD_TYPE}-${BOOTLOADER};base_dir=${CMAKE_SOURCE_DIR};absolute_paths_in_stderr=true;ignore_options=--specs=nano.specs --specs=nosys.specs"
        )
    set(CMAKE_C_COMPILER_LAUNCHER "${CMAKE_CXX_COMPILER_LAUNCHER}")
  endif()
else()
  message(STATUS "CCache disabled")
endif()
