target_link_libraries(firmware PUBLIC xbuddy_extension)

target_sources(firmware PRIVATE cooling.cpp xbuddy_extension.cpp)
