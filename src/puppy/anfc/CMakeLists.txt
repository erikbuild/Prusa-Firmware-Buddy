target_link_options(firmware PUBLIC "-L" "${CMAKE_CURRENT_SOURCE_DIR}/link")

if(BOARD STREQUAL "ANFC")
  add_library(HAL_Config INTERFACE)
  target_compile_definitions(HAL_Config INTERFACE STM32C092xx USE_HAL_DRIVER)
  target_include_directories(HAL_Config INTERFACE config)
  add_library(STM32C0_HAL_Config ALIAS HAL_Config)

  target_compile_definitions(firmware PRIVATE STM32GENERIC STM32C0 STM32C0xx)

  # TODO: Add boot/noboot versions of linker script
  target_set_linker_script(firmware "${CMAKE_CURRENT_SOURCE_DIR}/link/stm32c092.ld")

  target_sources(firmware PRIVATE startup/stm32c092.s system_stm32c0xx.c)

  set(HAL_LIB "STM32C0::HAL")

elseif(BOARD STREQUAL "ANFC_H5")
  add_library(HAL_Config INTERFACE)
  target_compile_definitions(HAL_Config INTERFACE STM32H503xx USE_HAL_DRIVER)
  target_include_directories(HAL_Config INTERFACE config)
  add_library(STM32H5_HAL_Config ALIAS HAL_Config)

  target_compile_definitions(firmware PRIVATE STM32GENERIC STM32H5 STM32H5xx)

  # TODO: This is wrong
  if(BOOTLOADER STREQUAL "EMPTY" OR BOOTLOADER)
    target_set_linker_script(firmware "${CMAKE_CURRENT_SOURCE_DIR}/link/stm32h503_boot.ld")
    target_compile_definitions(firmware PRIVATE BOOTLOADER_SIZE=8192)
  else()
    target_set_linker_script(firmware "${CMAKE_CURRENT_SOURCE_DIR}/link/stm32h503_noboot.ld")
    target_compile_definitions(firmware PRIVATE BOOTLOADER_SIZE=0)
  endif()

  target_sources(firmware PRIVATE startup/stm32h503.s system_stm32h5xx.c)

  set(HAL_LIB "STM32H5::HAL")
else()
  message(FATAL_ERROR "Unsupported nfc board: ${BOARD}")
endif()

target_sources(firmware PRIVATE hal.cpp main.cpp nfc.cpp stdlib.c)

target_link_libraries(firmware PRIVATE ${HAL_LIB} HAL_Config printf::printf SG14)
