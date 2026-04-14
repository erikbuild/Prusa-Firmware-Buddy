# This module needs to be object library, to prevent LTO+linker not use provided functions
add_library(cortex-m0-atomic OBJECT src/cm0-atomic.cpp)
target_link_libraries(cortex-m0-atomic PRIVATE CMSIS_common)
