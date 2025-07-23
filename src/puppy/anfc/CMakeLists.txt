if(BOARD STREQUAL "ANFC_C0")
  set(DEVICE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/device/stm32c0")

elseif(BOARD STREQUAL "ANFC_H5")
  set(DEVICE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/device/stm32h5")

else()
  message(FATAL_ERROR "Unsupported nfc board: ${BOARD}")
endif()

add_subdirectory("${DEVICE_DIR}")
target_sources(firmware PRIVATE "${DEVICE_DIR}/startup.s" "${DEVICE_DIR}/system.c")

target_link_options(firmware PUBLIC "-L" "${DEVICE_DIR}")
if(BOOTLOADER STREQUAL "EMPTY" OR BOOTLOADER)
  target_set_linker_script(firmware "${DEVICE_DIR}/link_boot.ld")
else()
  target_set_linker_script(firmware "${DEVICE_DIR}/link_noboot.ld")
  # Override bootloader size set in device CMakeLists
  set(BOOTLOADER_SIZE 0)
endif()
target_compile_definitions(firmware PRIVATE BOOTLOADER_SIZE=${BOOTLOADER_SIZE})

target_sources(
  firmware
  PRIVATE "${CMAKE_SOURCE_DIR}/src/device/multi_watchdog.cpp"
          anfc_event_publisher.cpp
          cyphal_anfc_node.cpp
          enum_mapping_check.cpp
          main.cpp
          nfc.cpp
          stdlib.cpp
  )

target_include_directories(
  firmware PRIVATE . ${CMAKE_SOURCE_DIR}/include stub ../shared/hal ../honeybee_shared
                   "${DEVICE_DIR}"
  )

target_link_libraries(
  firmware
  PRIVATE ${HAL_LIB}
          HAL_Config
          printf::printf
          SG14
          buddy_utils
          freertos
          dsdl_uavcan
          dsdl_prusa3d
          canard
          BuddyHeaders
          o1heap
          prusa_nfc_reader
          prusa_nfc_task
          st25r39xxb
          prusa_nfc_nfcv
          raii
  )

# TODO For now, we need to also explicitly depend on `FreeRTOS::FreeRTOS` because we are using
# functions that are not wrapped by `freertos`.
target_link_libraries(firmware PRIVATE FreeRTOS::FreeRTOS)

add_library(FreeRTOS_Config INTERFACE)
target_link_libraries(FreeRTOS_Config INTERFACE ${CMSIS_LIB})
target_include_directories(FreeRTOS_Config INTERFACE config)
