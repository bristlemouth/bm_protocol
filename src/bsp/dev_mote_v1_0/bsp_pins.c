#include "bsp.h"
#include "stm32_io.h"
#include "pca9535.h"

IOPinHandle_t GPIO2 = {&STM32PinDriver, &(STM32Pin_t){GPIO2_GPIO_Port, GPIO2_Pin, NULL, NULL}};
IOPinHandle_t GPIO1 = {&STM32PinDriver, &(STM32Pin_t){GPIO1_GPIO_Port, GPIO1_Pin, NULL, NULL}};
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
IOPinHandle_t ADIN_NSS = {&STM32PinDriver, &(STM32Pin_t){ADIN_NSS_GPIO_Port, ADIN_NSS_Pin, NULL, NULL}};
IOPinHandle_t ADIN_SCK = {&STM32PinDriver, &(STM32Pin_t){ADIN_SCK_GPIO_Port, ADIN_SCK_Pin, NULL, NULL}};
IOPinHandle_t ADIN_MISO = {&STM32PinDriver, &(STM32Pin_t){ADIN_MISO_GPIO_Port, ADIN_MISO_Pin, NULL, NULL}};
IOPinHandle_t ADIN_MOSI = {&STM32PinDriver, &(STM32Pin_t){ADIN_MOSI_GPIO_Port, ADIN_MOSI_Pin, NULL, NULL}};
IOPinHandle_t BOOT_LED = {&STM32PinDriver, &(STM32Pin_t){BOOT_LED_GPIO_Port, BOOT_LED_Pin, NULL, NULL}};
IOPinHandle_t ADIN_INT = {&STM32PinDriver, &(STM32Pin_t){ADIN_INT_GPIO_Port, ADIN_INT_Pin, NULL, NULL}};
IOPinHandle_t DEBUG_RX = {&STM32PinDriver, &(STM32Pin_t){DEBUG_RX_GPIO_Port, DEBUG_RX_Pin, NULL, NULL}};
IOPinHandle_t DEBUG_TX = {&STM32PinDriver, &(STM32Pin_t){DEBUG_TX_GPIO_Port, DEBUG_TX_Pin, NULL, NULL}};
IOPinHandle_t PAYLOAD_RX = {&STM32PinDriver, &(STM32Pin_t){PAYLOAD_RX_GPIO_Port, PAYLOAD_RX_Pin, NULL, NULL}};
IOPinHandle_t PAYLOAD_TX = {&STM32PinDriver, &(STM32Pin_t){PAYLOAD_TX_GPIO_Port, PAYLOAD_TX_Pin, NULL, NULL}};

extern PCA9535Device_t devMoteIOExpander;

IOPinHandle_t EXP_LED_G1 = {&PCA9535PinDriver, &(PCA9535Pin_t){&devMoteIOExpander, 0, PCA9535_MODE_OUTPUT}};
IOPinHandle_t EXP_LED_R1 = {&PCA9535PinDriver, &(PCA9535Pin_t){&devMoteIOExpander, 1, PCA9535_MODE_OUTPUT}};
IOPinHandle_t EXP_LED_G2 = {&PCA9535PinDriver, &(PCA9535Pin_t){&devMoteIOExpander, 2, PCA9535_MODE_OUTPUT}};
IOPinHandle_t EXP_LED_R2 = {&PCA9535PinDriver, &(PCA9535Pin_t){&devMoteIOExpander, 3, PCA9535_MODE_OUTPUT}};
IOPinHandle_t VUSB_DETECT = {&PCA9535PinDriver, &(PCA9535Pin_t){&devMoteIOExpander, 4, PCA9535_MODE_INPUT}};
IOPinHandle_t ADIN_RST = {&PCA9535PinDriver, &(PCA9535Pin_t){&devMoteIOExpander, 5, PCA9535_MODE_OUTPUT}};
IOPinHandle_t EXP_GPIO3 = {&PCA9535PinDriver, &(PCA9535Pin_t){&devMoteIOExpander, 6, PCA9535_MODE_OUTPUT}};
IOPinHandle_t EXP_GPIO4 = {&PCA9535PinDriver, &(PCA9535Pin_t){&devMoteIOExpander, 7, PCA9535_MODE_OUTPUT}};
IOPinHandle_t EXP_GPIO5 = {&PCA9535PinDriver, &(PCA9535Pin_t){&devMoteIOExpander, 8, PCA9535_MODE_OUTPUT}};
IOPinHandle_t EXP_GPIO6 = {&PCA9535PinDriver, &(PCA9535Pin_t){&devMoteIOExpander, 9, PCA9535_MODE_OUTPUT}};
IOPinHandle_t EXP_GPIO7 = {&PCA9535PinDriver, &(PCA9535Pin_t){&devMoteIOExpander, 10, PCA9535_MODE_OUTPUT}};
IOPinHandle_t EXP_GPIO8 = {&PCA9535PinDriver, &(PCA9535Pin_t){&devMoteIOExpander, 11, PCA9535_MODE_OUTPUT}};
IOPinHandle_t EXP_GPIO9 = {&PCA9535PinDriver, &(PCA9535Pin_t){&devMoteIOExpander, 12, PCA9535_MODE_OUTPUT}};
IOPinHandle_t EXP_GPIO10 = {&PCA9535PinDriver, &(PCA9535Pin_t){&devMoteIOExpander, 13, PCA9535_MODE_OUTPUT}};
IOPinHandle_t EXP_GPIO11 = {&PCA9535PinDriver, &(PCA9535Pin_t){&devMoteIOExpander, 14, PCA9535_MODE_OUTPUT}};
IOPinHandle_t EXP_GPIO12 = {&PCA9535PinDriver, &(PCA9535Pin_t){&devMoteIOExpander, 15, PCA9535_MODE_OUTPUT}};
