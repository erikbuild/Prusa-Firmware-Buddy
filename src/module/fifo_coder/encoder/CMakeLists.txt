add_library(fifo_encoder STATIC src/fifo_encoder.cpp)
target_include_directories(fifo_encoder PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(fifo_encoder PUBLIC fifo_coder_base)
