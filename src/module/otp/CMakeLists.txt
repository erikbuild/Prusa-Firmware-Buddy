add_library(otp STATIC src/types.cpp)
target_include_directories(otp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
