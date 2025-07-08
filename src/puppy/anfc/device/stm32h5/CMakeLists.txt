add_library(HAL_Config INTERFACE)
target_compile_definitions(HAL_Config INTERFACE STM32H503xx USE_HAL_DRIVER)
target_include_directories(HAL_Config INTERFACE config)
add_library(STM32H5_HAL_Config ALIAS HAL_Config)

target_compile_definitions(firmware PRIVATE STM32GENERIC STM32H5 STM32H5xx)

target_sources(firmware PRIVATE hal.cpp)

set(HAL_LIB
    "STM32H5::HAL"
    PARENT_SCOPE
    )
set(CMSIS_LIB
    "STM32H5xx::CMSIS"
    PARENT_SCOPE
    )
set(BOOTLOADER_SIZE
    32768
    PARENT_SCOPE
    )
