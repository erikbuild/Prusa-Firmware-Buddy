if(NOT BOARD STREQUAL "INDX_HEAD")
  message(FATAL_ERROR "Unsupported indx_head board: ${BOARD}")
endif()

target_link_options(firmware PUBLIC "-L" "${CMAKE_CURRENT_SOURCE_DIR}/link")
if(BOOTLOADER STREQUAL "EMPTY" OR BOOTLOADER)
  target_set_linker_script(firmware "${CMAKE_CURRENT_SOURCE_DIR}/link/link_boot.ld")
  set(BOOTLOADER_SIZE 8192)
else()
  target_set_linker_script(firmware "${CMAKE_CURRENT_SOURCE_DIR}/link/link_noboot.ld")
  set(BOOTLOADER_SIZE 0)
endif()

target_compile_definitions(
  firmware PRIVATE BOOTLOADER_SIZE=${BOOTLOADER_SIZE} STM32GENERIC STM32C0 STM32C0xx
  )

add_library(HAL_Config INTERFACE)
target_compile_definitions(HAL_Config INTERFACE STM32C092xx USE_HAL_DRIVER USE_FULL_LL_DRIVER)
target_include_directories(HAL_Config INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/config")
add_library(STM32C0_HAL_Config ALIAS HAL_Config)

target_sources(
  firmware
  PRIVATE src/app.cpp
          src/hal.cpp
          src/hal_adc.cpp
          src/hal_clk.cpp
          src/hal_crc.cpp
          src/hal_i2c.cpp
          src/hal_spi.cpp
          src/hal_tim.cpp
          src/hal_watchdog.cpp
          src/heater.cpp
          src/main.cpp
          src/rtt.cpp
          src/startup.s
          src/stdlib.cpp
          src/system.c
          src/timing.cpp
          src/watchdog.cpp
  )

target_link_libraries(
  firmware
  PRIVATE STM32C0::HAL
          HAL_Config
          freertos
          indx_head
          bsod
          raii
          fpm
          ADS131M02
          AIS2IH
          LP5817
          cortex-m0-atomic
  )

target_include_directories(firmware PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

# TODO For now, we need to also explicitly depend on `FreeRTOS::FreeRTOS` because we are using
# functions that are not wrapped by `freertos`.
target_link_libraries(firmware PRIVATE FreeRTOS::FreeRTOS)

add_library(FreeRTOS_Config INTERFACE)
target_link_libraries(FreeRTOS_Config INTERFACE STM32C0xx::CMSIS)
target_include_directories(FreeRTOS_Config INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/config")
