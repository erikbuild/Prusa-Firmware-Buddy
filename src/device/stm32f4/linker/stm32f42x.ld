/* STM32F42x layout without a bootloader */

FLASH_ORIGIN = 0x08000000;
FLASH_LENGTH = 2048K;
RAM_SIZE_TOTAL = 192K;

INCLUDE stm32f4_platform.ld;
