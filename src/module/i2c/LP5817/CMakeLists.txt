add_library(LP5817 INTERFACE)
target_include_directories(LP5817 INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(LP5817 INTERFACE i2c_base)
