#include "bsp.h"
#include "stm32_io.h"

IOPinHandle_t VUSB_DETECT = {&STM32PinDriver, &(STM32Pin_t){VUSB_DETECT_GPIO_Port, VUSB_DETECT_Pin, NULL, NULL}};
IOPinHandle_t DISCHARGE_ON = {&STM32PinDriver, &(STM32Pin_t){DISCHARGE_ON_GPIO_Port, DISCHARGE_ON_Pin, NULL, NULL}};
IOPinHandle_t PL_BUCK_EN = {&STM32PinDriver, &(STM32Pin_t){PL_BUCK_EN_GPIO_Port, PL_BUCK_EN_Pin, NULL, NULL}};
IOPinHandle_t VBUS_EN = {&STM32PinDriver, &(STM32Pin_t){VBUS_EN_GPIO_Port, VBUS_EN_Pin, NULL, NULL}};
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
IOPinHandle_t PAYLOAD_RX = {&STM32PinDriver, &(STM32Pin_t){PAYLOAD_RX_GPIO_Port, PAYLOAD_RX_Pin, NULL, NULL}};
IOPinHandle_t PAYLOAD_TX = {&STM32PinDriver, &(STM32Pin_t){PAYLOAD_TX_GPIO_Port, PAYLOAD_TX_Pin, NULL, NULL}};
