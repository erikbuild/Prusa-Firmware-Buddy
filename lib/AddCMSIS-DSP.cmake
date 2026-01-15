add_subdirectory(CMSIS-DSP)
target_link_libraries(CMSISDSP PUBLIC CMSIS_common)
target_compile_options(CMSISDSP PRIVATE -Wno-float-conversion)
