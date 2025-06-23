# st25r39xxb INFCReader implementation for PrusaNFCReader

add_library(prusa_nfc_nfcv)
target_sources(prusa_nfc_nfcv PRIVATE prusa_nfc_nfcv/ll_nfc_reader.cpp)
target_include_directories(prusa_nfc_nfcv PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(prusa_nfc_nfcv PUBLIC prusa_nfc_reader nfcv SG14 buddy_utils raii)
