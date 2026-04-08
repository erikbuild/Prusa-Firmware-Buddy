add_library(tpis STATIC src/tpis/tpis.cpp)

target_include_directories(tpis PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
