#pragma once
#include <device/hal.h>
#include <device/board.h>
#include "printers.h"

#include <option/has_esp.h>

#if (BOARD_IS_XBUDDY() || BOARD_IS_XLBUDDY())
    #define HAS_ADC3
#endif

//
// I2C
//

extern I2C_HandleTypeDef hi2c1;
extern I2C_HandleTypeDef hi2c2;
extern I2C_HandleTypeDef hi2c3;

//
// SPI
//

extern SPI_HandleTypeDef hspi2;
extern SPI_HandleTypeDef hspi3;
extern SPI_HandleTypeDef hspi4;
extern SPI_HandleTypeDef hspi5;
extern SPI_HandleTypeDef hspi6;

//
// ADCs
//

extern ADC_HandleTypeDef hadc1;
extern ADC_HandleTypeDef hadc2;
extern ADC_HandleTypeDef hadc3;

//
// Timers
//

extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim3;
extern TIM_HandleTypeDef htim4;
extern TIM_HandleTypeDef htim8;
extern TIM_HandleTypeDef htim9;
extern TIM_HandleTypeDef htim13;
extern TIM_HandleTypeDef htim14;

//
// GPIO
//
// TODO: Migrate GPIO to Pin.hpp

#define USB_HS_N_Pin       GPIO_PIN_14
#define USB_HS_N_GPIO_Port GPIOB
#define USB_HS_P_Pin       GPIO_PIN_15
#define USB_HS_P_GPIO_Port GPIOB
#define THERM_0_Pin        GPIO_PIN_0
#define THERM_0_GPIO_Port  GPIOC

#define BUZZER_Pin       GPIO_PIN_0
#define BUZZER_GPIO_Port GPIOA

#define HW_IDENTIFY_Pin       GPIO_PIN_3
#define HW_IDENTIFY_GPIO_Port GPIOA
#define THERM_1_Pin           GPIO_PIN_4
#define THERM_1_GPIO_Port     GPIOA

#if (BOARD_IS_BUDDY())
    #if (HAS_ESP())
        #define ESP_TX_Pin       GPIO_PIN_6
        #define ESP_TX_GPIO_Port GPIOC
        #define ESP_RX_Pin       GPIO_PIN_7
        #define ESP_RX_GPIO_Port GPIOC
    #endif
    #define THERM_2_Pin           GPIO_PIN_5
    #define THERM_2_GPIO_Port     GPIOA
    #define THERM_PINDA_Pin       GPIO_PIN_6
    #define THERM_PINDA_GPIO_Port GPIOA
#else
    #define THERM_2_Pin       GPIO_PIN_10
    #define THERM_2_GPIO_Port GPIOF
#endif

#if (BOARD_IS_XBUDDY() && !PRINTER_IS_PRUSA_MK3_5())
    #define THERM_HEATBREAK_Pin       GPIO_PIN_6
    #define THERM_HEATBREAK_GPIO_Port GPIOA
#endif

#define BED_HEAT_Pin         GPIO_PIN_0
#define BED_HEAT_GPIO_Port   GPIOB
#define HEAT0_Pin            GPIO_PIN_1
#define HEAT0_GPIO_Port      GPIOB
#define USB_FS_N_Pin         GPIO_PIN_11
#define USB_FS_N_GPIO_Port   GPIOA
#define USB_FS_P_Pin         GPIO_PIN_12
#define USB_FS_P_GPIO_Port   GPIOA
#define FLASH_SCK_Pin        GPIO_PIN_10
#define FLASH_SCK_GPIO_Port  GPIOC
#define FLASH_MISO_Pin       GPIO_PIN_11
#define FLASH_MISO_GPIO_Port GPIOC
#define FLASH_MOSI_Pin       GPIO_PIN_12
#define FLASH_MOSI_GPIO_Port GPIOC
#define TX1_Pin              GPIO_PIN_6
#define TX1_GPIO_Port        GPIOB
#define RX1_Pin              GPIO_PIN_7
#define RX1_GPIO_Port        GPIOB
#define MMU_TX_Pin           GPIO_PIN_6
#define MMU_TX_GPIO_Port     GPIOC
#define MMU_RX_Pin           GPIO_PIN_7
#define MMU_RX_GPIO_Port     GPIOC

#define BED_MON_Pin       GPIO_PIN_3
#define BED_MON_GPIO_Port GPIOA

#define HEATER_CURRENT_Pin       GPIO_PIN_3
#define HEATER_CURRENT_GPIO_Port GPIOF

#define INPUT_CURRENT_Pin       GPIO_PIN_4
#define INPUT_CURRENT_GPIO_Port GPIOF
#define THERM3_Pin              GPIO_PIN_5
#define THERM3_GPIO_Port        GPIOF
#define MMU_CURRENT_Pin         GPIO_PIN_6
#define MMU_CURRENT_GPIO_Port   GPIOF

#define HEATER_VOLTAGE_Pin       GPIO_PIN_3
#define HEATER_VOLTAGE_GPIO_Port GPIOA

#define BED_VOLTAGE_Pin       GPIO_PIN_5
#define BED_VOLTAGE_GPIO_Port GPIOA

#define USB_OVERC_Pin       GPIO_PIN_4
#define USB_OVERC_GPIO_Port GPIOE

#if HAS_ESP()
    #if (BOARD_IS_XBUDDY() || BOARD_IS_XLBUDDY())
        #define ESP_GPIO0_Pin GPIO_PIN_15
    #else
        #define ESP_GPIO0_Pin GPIO_PIN_6
    #endif
    #define ESP_GPIO0_GPIO_Port GPIOE
    #define ESP_RST_Pin         GPIO_PIN_13
    #define ESP_RST_GPIO_Port   GPIOC
#endif

#define BED_MON_Pin                 GPIO_PIN_3
#define BED_MON_GPIO_Port           GPIOA
#define FANPRINT_TACH_Pin           GPIO_PIN_10
#define FANPRINT_TACH_GPIO_Port     GPIOE
#define FANHEATBREAK_TACH_Pin       GPIO_PIN_14
#define FANHEATBREAK_TACH_GPIO_Port GPIOE
#define SWDIO_Pin                   GPIO_PIN_13
#define SWDIO_GPIO_Port             GPIOA
#define SWCLK_Pin                   GPIO_PIN_14
#define SWCLK_GPIO_Port             GPIOA
#define WP2_Pin                     GPIO_PIN_5
#define WP2_GPIO_Port               GPIOB
#define WP1_Pin                     GPIO_PIN_0
#define WP1_GPIO_Port               GPIOE

#if (BOARD_IS_XBUDDY() || BOARD_IS_XLBUDDY())
    #define i2c2_SDA_PORT_BASE GPIOF_BASE
    #define i2c2_SCL_PORT_BASE GPIOF_BASE
    #define i2c2_SDA_PORT      ((GPIO_TypeDef *)i2c2_SDA_PORT_BASE)
    #define i2c2_SCL_PORT      ((GPIO_TypeDef *)i2c2_SCL_PORT_BASE)
    #define i2c2_SDA_PIN       GPIO_PIN_0
    #define i2c2_SCL_PIN       GPIO_PIN_1

    // iX doesn't have touchscreen
    #if !PRINTER_IS_PRUSA_iX()
        #define i2c3_SDA_PORT_BASE GPIOC_BASE
        #define i2c3_SCL_PORT_BASE GPIOA_BASE
        #define i2c3_SDA_PORT      ((GPIO_TypeDef *)i2c3_SDA_PORT_BASE)
        #define i2c3_SCL_PORT      ((GPIO_TypeDef *)i2c3_SCL_PORT_BASE)
        #define i2c3_SDA_PIN       GPIO_PIN_9
        #define i2c3_SCL_PIN       GPIO_PIN_8
    #endif
#endif

#if (BOARD_IS_XLBUDDY())
    #define i2c1_SDA_PORT_BASE GPIOB_BASE
    #define i2c1_SCL_PORT_BASE GPIOB_BASE
    #define i2c1_SDA_PORT      ((GPIO_TypeDef *)i2c1_SDA_PORT_BASE)
    #define i2c1_SCL_PORT      ((GPIO_TypeDef *)i2c1_SCL_PORT_BASE)
    #define i2c1_SDA_PIN       GPIO_PIN_7
    #define i2c1_SCL_PIN       GPIO_PIN_6
#endif

#if (BOARD_IS_BUDDY())
    #define i2c1_SDA_PORT_BASE GPIOB_BASE
    #define i2c1_SCL_PORT_BASE GPIOB_BASE
    #define i2c1_SDA_PORT      ((GPIO_TypeDef *)i2c1_SDA_PORT_BASE)
    #define i2c1_SCL_PORT      ((GPIO_TypeDef *)i2c1_SCL_PORT_BASE)
    #define i2c1_SDA_PIN       GPIO_PIN_9
    #define i2c1_SCL_PIN       GPIO_PIN_8
#endif

//
// External Peripherals Assignment
//

#if BOARD_IS_BUDDY()

constexpr I2C_HandleTypeDef *i2c_handle_eeprom = &hi2c1;
constexpr I2C_HandleTypeDef *i2c_handle_gcode = &hi2c1;
    #define i2c_init_eeprom hw_i2c1_init
    #define i2c_init_gcode  hw_i2c1_init
    #define HAS_I2C1()      1
    #define HAS_I2C2()      0
    #define HAS_I2C3()      0

constexpr SPI_HandleTypeDef *spi_handle_lcd = &hspi2;
constexpr SPI_HandleTypeDef *spi_handle_flash = &hspi3;
constexpr SPI_HandleTypeDef *spi_handle_accelerometer = nullptr;
    #define spi_init_lcd   hw_spi2_init
    #define spi_init_flash hw_spi3_init

#elif BOARD_IS_XBUDDY()

constexpr I2C_HandleTypeDef *i2c_handle_eeprom = &hi2c2;
constexpr I2C_HandleTypeDef *i2c_handle_usbc = &hi2c2;
constexpr I2C_HandleTypeDef *i2c_handle_gcode = &hi2c2;
constexpr I2C_HandleTypeDef *i2c_handle_io_expander2 = &hi2c2;
    #define i2c_init_eeprom        hw_i2c2_init
    #define i2c_init_usbc          hw_i2c2_init
    #define i2c_init_gcode         hw_i2c2_init
    #define i2c_init_io_expander2  hw_i2c2_init
    #define HAS_I2C1()             0
    #define HAS_I2C2()             1

constexpr SPI_HandleTypeDef *spi_handle_accelerometer = &hspi2;
constexpr SPI_HandleTypeDef *spi_handle_tmc = &hspi3;
constexpr SPI_HandleTypeDef *spi_handle_flash = &hspi5;
constexpr SPI_HandleTypeDef *spi_handle_lcd = &hspi6;
    #define spi_init_accelerometer hw_spi2_init
    #define spi_init_tmc           hw_spi3_init
    #define spi_init_flash         hw_spi5_init
    #define spi_init_lcd           hw_spi6_init

constexpr TIM_HandleTypeDef *tim_handle_burst_stepping = &htim8;
constexpr TIM_HandleTypeDef *tim_handle_phase_stepping = &htim13;

    #if PRINTER_IS_PRUSA_iX()
        #define HAS_I2C3()   0

constexpr SPI_HandleTypeDef *spi_handle_led = &hspi4;
        #define spi_init_led hw_spi4_init
    #else
constexpr I2C_HandleTypeDef *i2c_handle_touch = &hi2c3;
        #define i2c_init_touch hw_i2c3_init
        #define HAS_I2C3()     1
    #endif

#elif BOARD_IS_XLBUDDY()

constexpr I2C_HandleTypeDef *i2c_handle_usbc = &hi2c1;
constexpr I2C_HandleTypeDef *i2c_handle_eeprom = &hi2c2;
constexpr I2C_HandleTypeDef *i2c_handle_gcode = &hi2c2;
constexpr I2C_HandleTypeDef *i2c_handle_io_expander1 = &hi2c2;
constexpr I2C_HandleTypeDef *i2c_handle_touch = &hi2c3;
    #define i2c_init_usbc          hw_i2c1_init
    #define i2c_init_eeprom        hw_i2c2_init
    #define i2c_init_gcode         hw_i2c2_init
    #define i2c_init_io_expander1  hw_i2c2_init
    #define i2c_init_touch         hw_i2c3_init
    #define HAS_I2C1()             1
    #define HAS_I2C2()             1
    #define HAS_I2C3()             1

constexpr SPI_HandleTypeDef *spi_handle_accelerometer = &hspi2;
constexpr SPI_HandleTypeDef *spi_handle_tmc = &hspi3;
// Side LEDs use either SPI4 or share SPI with LCD, depending on HW revision
constexpr SPI_HandleTypeDef *spi_handle_led = &hspi4;
constexpr SPI_HandleTypeDef *spi_handle_flash = &hspi5;
constexpr SPI_HandleTypeDef *spi_handle_lcd = &hspi6;
    #define spi_init_accelerometer hw_spi2_init
    #define spi_init_tmc           hw_spi3_init
    #define spi_init_led           hw_spi4_init
    #define spi_init_flash         hw_spi5_init
    #define spi_init_lcd           hw_spi6_init

constexpr TIM_HandleTypeDef *tim_handle_burst_stepping = &htim8;
constexpr TIM_HandleTypeDef *tim_handle_phase_stepping = &htim13;

#else
    #define HAS_I2C1() 0
    #define HAS_I2C2() 0
    #define HAS_I2C3() 0
    #error Unknown board
#endif

//
// Other
//

extern RTC_HandleTypeDef hrtc;
extern RNG_HandleTypeDef hrng;

//
// Initialization
//
void hw_rtc_init();
void hw_rng_init();

void hw_gpio_init();
void hw_dma_init();

void hw_adc1_init();
void hw_adc3_init();
void hw_adc_irq_init();

//
// i2c init may not exist without External Peripherals Assignment
// it contains busy flag clear function, and it requires pyhsical pins to compile
// i2c init is also used to clear BUSY flag
//

#if HAS_I2C1()
void hw_i2c1_init();
void hw_i2c1_pins_init();
#endif

#if HAS_I2C2()
void hw_i2c2_init();
void hw_i2c2_pins_init();
#endif

#if HAS_I2C3()
void hw_i2c3_init();
void hw_i2c3_pins_init();
#endif

void hw_spi2_init();
void hw_spi3_init();
void hw_spi4_init();
void hw_spi5_init();
void hw_spi6_init();

void hw_tim1_init();
void hw_tim2_init();
void hw_tim3_init();
void hw_tim4_init();
void hw_tim8_init();
void hw_tim9_init();
void hw_tim13_init();
void hw_tim14_init();
