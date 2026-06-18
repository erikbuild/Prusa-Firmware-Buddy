/* STM32F407VG layout without a bootloader */

FLASH_ORIGIN = 0x08000000;
FLASH_LENGTH = 1024K;
RAM_SIZE_TOTAL = 128K;

INCLUDE stm32f4_platform.ld;
