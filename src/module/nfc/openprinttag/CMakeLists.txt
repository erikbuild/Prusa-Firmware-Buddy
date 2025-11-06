add_library(openprinttag STATIC)
target_include_directories(openprinttag PUBLIC .)
target_link_libraries(openprinttag PRIVATE nanocbor SG14 buddy_utils)
add_subdirectory(openprinttag)
