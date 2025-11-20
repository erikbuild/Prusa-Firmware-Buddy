add_library(accelerometer STATIC common_structs.cpp)

target_include_directories(accelerometer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
