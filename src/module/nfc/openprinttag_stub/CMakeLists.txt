# Stub OPTBackend implementation for OPTReader

add_library(openprinttag_stub STATIC)
target_sources(openprinttag_stub PRIVATE openprinttag_stub/opt_stub.cpp)
target_include_directories(openprinttag_stub PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(openprinttag_stub PUBLIC openprinttag SG14 buddy_utils raii)
