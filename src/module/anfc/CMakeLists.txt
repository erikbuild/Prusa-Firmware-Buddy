add_library(anfc STATIC src/anfc/modbus.cpp src/anfc/types.cpp)
target_link_libraries(anfc PRIVATE dsdl_prusa3d dsdl_uavcan)
target_link_libraries(anfc PUBLIC modbus)
target_include_directories(anfc PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
