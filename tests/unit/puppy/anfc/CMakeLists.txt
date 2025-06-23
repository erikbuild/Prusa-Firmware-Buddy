add_executable(
  test_ll_nfc_reader
  ${CMAKE_SOURCE_DIR}/src/module/nfc/prusa_nfc_nfcv/prusa_nfc_nfcv/ll_nfc_reader.cpp
  test_ll_nfc_reader.cpp
  )
target_link_libraries(test_ll_nfc_reader nfcv buddy_utils raii prusa_nfc_reader SG14)
target_include_directories(
  test_ll_nfc_reader PRIVATE ${CMAKE_SOURCE_DIR}/src/module/nfc/prusa_nfc_nfcv
  )
add_catch_test(test_ll_nfc_reader)
