add_library(
  nanocbor
  ${CMAKE_CURRENT_SOURCE_DIR}/nanocbor/src/decoder.c
  ${CMAKE_CURRENT_SOURCE_DIR}/nanocbor/src/encoder.c
  ${CMAKE_CURRENT_SOURCE_DIR}/nanocbor/src/endian.cpp
  )

target_include_directories(nanocbor PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/nanocbor/include/)
