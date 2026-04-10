add_library(fifo_coder_base INTERFACE)
target_include_directories(fifo_coder_base INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_subdirectory(encoder)
add_subdirectory(decoder)
