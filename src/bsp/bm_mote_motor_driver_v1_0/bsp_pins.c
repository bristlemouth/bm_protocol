#include "bsp.h"
#include "stm32_io.h"
#include "pca9535.h"

IOPinHandle_t IMU_INT = {&STM32PinDriver, &(STM32Pin_t){IMU_INT_GPIO_Port, IMU_INT_Pin, NULL, NULL}};
IOPinHandle_t VUSB_DETECT = {&STM32PinDriver, &(STM32Pin_t){VUSB_DETECT_GPIO_Port, VUSB_DETECT_Pin, NULL, NULL}};
IOPinHandle_t POWER_EN = {&STM32PinDriver, &(STM32Pin_t){POWER_EN_GPIO_Port, POWER_EN_Pin, NULL, NULL}};
IOPinHandle_t MOTOR_SPEED1 = {&STM32PinDriver, &(STM32Pin_t){MOTOR_SPEED1_GPIO_Port, MOTOR_SPEED1_Pin, NULL, NULL}};
IOPinHandle_t BM_CS = {&STM32PinDriver, &(STM32Pin_t){BM_SPI_CS_GPIO_Port, BM_SPI_CS_Pin, NULL, NULL}};
IOPinHandle_t BM_SCK = {&STM32PinDriver, &(STM32Pin_t){BM_SPI_SCK_GPIO_Port, BM_SPI_SCK_Pin, NULL, NULL}};
IOPinHandle_t BM_MISO = {&STM32PinDriver, &(STM32Pin_t){BM_SPI_MISO_GPIO_Port, BM_SPI_MISO_Pin, NULL, NULL}};
IOPinHandle_t BM_MOSI = {&STM32PinDriver, &(STM32Pin_t){BM_SPI_MOSI_GPIO_Port, BM_SPI_MOSI_Pin, NULL, NULL}};
IOPinHandle_t MOTOR_SPEED2 = {&STM32PinDriver, &(STM32Pin_t){MOTOR_SPEED2_GPIO_Port, MOTOR_SPEED2_Pin, NULL, NULL}};
IOPinHandle_t BB_VBUS_EN = {&STM32PinDriver, &(STM32Pin_t){BB_VBUS_EN_GPIO_Port, BB_VBUS_EN_Pin, NULL, NULL}};
IOPinHandle_t FLASH_SCK = {&STM32PinDriver, &(STM32Pin_t){FLASH_SCK_GPIO_Port, FLASH_SCK_Pin, NULL, NULL}};
IOPinHandle_t FLASH_MISO = {&STM32PinDriver, &(STM32Pin_t){FLASH_MISO_GPIO_Port, FLASH_MISO_Pin, NULL, NULL}};
IOPinHandle_t FLASH_MOSI = {&STM32PinDriver, &(STM32Pin_t){FLASH_MOSI_GPIO_Port, FLASH_MOSI_Pin, NULL, NULL}};
IOPinHandle_t FLASH_CS= {&STM32PinDriver, &(STM32Pin_t){FLASH_CS_GPIO_Port, FLASH_CS_Pin, NULL, NULL}};
IOPinHandle_t ADIN_PWR = {&STM32PinDriver, &(STM32Pin_t){ADIN_PWR_GPIO_Port, ADIN_PWR_Pin, NULL, NULL}};
IOPinHandle_t ADIN_RST = {&STM32PinDriver, &(STM32Pin_t){ADIN_RST_GPIO_Port, ADIN_RST_Pin, NULL, NULL}};
IOPinHandle_t ADIN_CS = {&STM32PinDriver, &(STM32Pin_t){ADIN_CS_GPIO_Port, ADIN_CS_Pin, NULL, NULL}};
IOPinHandle_t ADIN_SCK = {&STM32PinDriver, &(STM32Pin_t){ADIN_SCK_GPIO_Port, ADIN_SCK_Pin, NULL, NULL}};
IOPinHandle_t ADIN_MISO = {&STM32PinDriver, &(STM32Pin_t){ADIN_MISO_GPIO_Port, ADIN_MISO_Pin, NULL, NULL}};
IOPinHandle_t ADIN_MOSI = {&STM32PinDriver, &(STM32Pin_t){ADIN_MOSI_GPIO_Port, ADIN_MOSI_Pin, NULL, NULL}};
IOPinHandle_t ADIN_INT = {&STM32PinDriver, &(STM32Pin_t){ADIN_INT_GPIO_Port, ADIN_INT_Pin, NULL, NULL}};
IOPinHandle_t BOOT_LED = {&STM32PinDriver, &(STM32Pin_t){BOOT_LED_GPIO_Port, BOOT_LED_Pin, NULL, NULL}};
IOPinHandle_t BARO_INT = {&STM32PinDriver, &(STM32Pin_t){BARO_INT_GPIO_Port, BARO_INT_Pin, NULL, NULL}};
