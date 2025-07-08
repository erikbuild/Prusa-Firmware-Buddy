add_library(HAL_Config INTERFACE)
target_compile_definitions(HAL_Config INTERFACE STM32C092xx USE_HAL_DRIVER)
target_include_directories(HAL_Config INTERFACE config)
add_library(STM32C0_HAL_Config ALIAS HAL_Config)

target_compile_definitions(firmware PRIVATE STM32GENERIC STM32C0 STM32C0xx)

target_sources(firmware PRIVATE atomic_c0.cpp hal.cpp)

# Required by self_hash
target_link_libraries(firmware PRIVATE mbedTLS)

set(HAL_LIB
    "STM32C0::HAL"
    PARENT_SCOPE
    )
set(CMSIS_LIB
    "STM32C0xx::CMSIS"
    PARENT_SCOPE
    )
set(BOOTLOADER_SIZE
    32768
    PARENT_SCOPE
    )
