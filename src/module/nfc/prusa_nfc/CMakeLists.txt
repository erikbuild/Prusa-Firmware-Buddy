add_library(prusa_nfc_reader)
target_include_directories(prusa_nfc_reader PUBLIC .)
target_link_libraries(prusa_nfc_reader PRIVATE nanocbor SG14 buddy_utils)
add_subdirectory(prusa_nfc)
