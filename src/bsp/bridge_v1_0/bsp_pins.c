#include "bsp.h"
#include "stm32_io.h"
#include "pca9535.h"

IOPinHandle_t ADIN_PWR = {&STM32PinDriver, &(STM32Pin_t){ADIN_PWR_GPIO_Port, ADIN_PWR_Pin, NULL, NULL}};
IOPinHandle_t BM_CS = {&STM32PinDriver, &(STM32Pin_t){BM_CS_GPIO_Port, BM_CS_Pin, NULL, NULL}};
IOPinHandle_t BM_SCK_RX3 = {&STM32PinDriver, &(STM32Pin_t){BM_SCK_RX3_GPIO_Port, BM_SCK_RX3_Pin, NULL, NULL}};
IOPinHandle_t BM_MISO = {&STM32PinDriver, &(STM32Pin_t){BM_MISO_GPIO_Port, BM_MISO_Pin, NULL, NULL}};
IOPinHandle_t BM_MOSI_TX3 = {&STM32PinDriver, &(STM32Pin_t){BM_MOSI_TX3_GPIO_Port, BM_MOSI_TX3_Pin, NULL, NULL}};
IOPinHandle_t BM_INT = {&STM32PinDriver, &(STM32Pin_t){BM_INT_GPIO_Port, BM_INT_Pin, NULL, NULL}}; // TODO setup int handlers?
IOPinHandle_t FLASH_SCK = {&STM32PinDriver, &(STM32Pin_t){FLASH_SCK_GPIO_Port, FLASH_SCK_Pin, NULL, NULL}};
IOPinHandle_t FLASH_MISO = {&STM32PinDriver, &(STM32Pin_t){FLASH_MISO_GPIO_Port, FLASH_MISO_Pin, NULL, NULL}};
IOPinHandle_t FLASH_MOSI = {&STM32PinDriver, &(STM32Pin_t){FLASH_MOSI_GPIO_Port, FLASH_MOSI_Pin, NULL, NULL}};
IOPinHandle_t FLASH_CS= {&STM32PinDriver, &(STM32Pin_t){FLASH_CS_GPIO_Port, FLASH_CS_Pin, NULL, NULL}};
IOPinHandle_t ADIN_CS = {&STM32PinDriver, &(STM32Pin_t){ADIN_CS_GPIO_Port, ADIN_CS_Pin, NULL, NULL}};
IOPinHandle_t ADIN_SCK = {&STM32PinDriver, &(STM32Pin_t){ADIN_SCK_GPIO_Port, ADIN_SCK_Pin, NULL, NULL}};
IOPinHandle_t ADIN_MISO = {&STM32PinDriver, &(STM32Pin_t){ADIN_MISO_GPIO_Port, ADIN_MISO_Pin, NULL, NULL}};
IOPinHandle_t ADIN_MOSI = {&STM32PinDriver, &(STM32Pin_t){ADIN_MOSI_GPIO_Port, ADIN_MOSI_Pin, NULL, NULL}};
IOPinHandle_t BOOT = {&STM32PinDriver, &(STM32Pin_t){BOOT_GPIO_Port, BOOT_Pin, NULL, NULL}};
IOPinHandle_t ADIN_INT = {&STM32PinDriver, &(STM32Pin_t){ADIN_INT_GPIO_Port, ADIN_INT_Pin, NULL, NULL}};
IOPinHandle_t DEBUG_RX = {&STM32PinDriver, &(STM32Pin_t){DEBUG_RX_GPIO_Port, DEBUG_RX_Pin, NULL, NULL}};
IOPinHandle_t DEBUG_TX = {&STM32PinDriver, &(STM32Pin_t){DEBUG_TX_GPIO_Port, DEBUG_TX_Pin, NULL, NULL}};
IOPinHandle_t BOOST_EN = {&STM32PinDriver, &(STM32Pin_t){BOOST_EN_GPIO_Port, BOOST_EN_Pin, NULL, NULL}};
IOPinHandle_t VBUS_SW_EN = {&STM32PinDriver, &(STM32Pin_t){VBUS_SW_EN_GPIO_Port, VBUS_SW_EN_Pin, NULL, NULL}};