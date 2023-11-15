#include "bsp.h"
#include "stm32_io.h"
#include "pca9535.h"

IOPinHandle_t GPIO2 = {&STM32PinDriver, &(STM32Pin_t){GPIO2_GPIO_Port, GPIO2_Pin, NULL, NULL}};
IOPinHandle_t GPIO1 = {&STM32PinDriver, &(STM32Pin_t){GPIO1_GPIO_Port, GPIO1_Pin, NULL, NULL}};
IOPinHandle_t VUSB_DETECT = {&STM32PinDriver, &(STM32Pin_t){VUSB_DETECT_GPIO_Port, VUSB_DETECT_Pin, NULL, NULL}};
IOPinHandle_t IOEXP_INT = {&STM32PinDriver, &(STM32Pin_t){IOEXP_INT_GPIO_Port, IOEXP_INT_Pin, NULL, NULL}};
IOPinHandle_t I2C_MUX_RESET = {&STM32PinDriver, &(STM32Pin_t){I2C_MUX_RESET_GPIO_Port, I2C_MUX_RESET_Pin, NULL, NULL}};
IOPinHandle_t BM_CS = {&STM32PinDriver, &(STM32Pin_t){BM_CS_GPIO_Port, BM_CS_Pin, NULL, NULL}};
IOPinHandle_t BM_SCK_RX3 = {&STM32PinDriver, &(STM32Pin_t){BM_SCK_RX3_GPIO_Port, BM_SCK_RX3_Pin, NULL, NULL}};
IOPinHandle_t BM_MISO = {&STM32PinDriver, &(STM32Pin_t){BM_MISO_GPIO_Port, BM_MISO_Pin, NULL, NULL}};
IOPinHandle_t BM_MOSI_TX3 = {&STM32PinDriver, &(STM32Pin_t){BM_MOSI_TX3_GPIO_Port, BM_MOSI_TX3_Pin, NULL, NULL}};
IOPinHandle_t BM_INT = {&STM32PinDriver, &(STM32Pin_t){BM_INT_GPIO_Port, BM_INT_Pin, NULL, NULL}}; // TODO setup int handlers?
IOPinHandle_t VBUS_BF_EN = {&STM32PinDriver, &(STM32Pin_t){VBUS_BF_EN_GPIO_Port, VBUS_BF_EN_Pin, NULL, NULL}};
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

extern PCA9535Device_t bristlefinIOExpander;

IOPinHandle_t BF_IO1 = {&PCA9535PinDriver, &(PCA9535Pin_t){&bristlefinIOExpander, 0, PCA9535_MODE_INPUT}};
IOPinHandle_t BF_IO2 = {&PCA9535PinDriver, &(PCA9535Pin_t){&bristlefinIOExpander, 1, PCA9535_MODE_INPUT}};
IOPinHandle_t BF_HFIO = {&PCA9535PinDriver, &(PCA9535Pin_t){&bristlefinIOExpander, 2, PCA9535_MODE_OUTPUT}};
IOPinHandle_t BF_3V3_EN = {&PCA9535PinDriver, &(PCA9535Pin_t){&bristlefinIOExpander, 3, PCA9535_MODE_OUTPUT}};
IOPinHandle_t BF_5V_EN = {&PCA9535PinDriver, &(PCA9535Pin_t){&bristlefinIOExpander, 4, PCA9535_MODE_OUTPUT}};
IOPinHandle_t BF_IMU_INT = {&PCA9535PinDriver, &(PCA9535Pin_t){&bristlefinIOExpander, 5, PCA9535_MODE_INPUT}};
IOPinHandle_t BF_IMU_RST = {&PCA9535PinDriver, &(PCA9535Pin_t){&bristlefinIOExpander, 6, PCA9535_MODE_OUTPUT}};
IOPinHandle_t BF_SDI12_OE = {&PCA9535PinDriver, &(PCA9535Pin_t){&bristlefinIOExpander, 7, PCA9535_MODE_OUTPUT}};
IOPinHandle_t BF_TP16 = {&PCA9535PinDriver, &(PCA9535Pin_t){&bristlefinIOExpander, 8, PCA9535_MODE_INPUT}};
IOPinHandle_t BF_LED_G1 = {&PCA9535PinDriver, &(PCA9535Pin_t){&bristlefinIOExpander, 9, PCA9535_MODE_OUTPUT}};
IOPinHandle_t BF_LED_R1 = {&PCA9535PinDriver, &(PCA9535Pin_t){&bristlefinIOExpander, 10, PCA9535_MODE_OUTPUT}};
IOPinHandle_t BF_LED_G2 = {&PCA9535PinDriver, &(PCA9535Pin_t){&bristlefinIOExpander, 11, PCA9535_MODE_OUTPUT}};
IOPinHandle_t BF_LED_R2 = {&PCA9535PinDriver, &(PCA9535Pin_t){&bristlefinIOExpander, 12, PCA9535_MODE_OUTPUT}};
IOPinHandle_t BF_PL_BUCK_EN = {&PCA9535PinDriver, &(PCA9535Pin_t){&bristlefinIOExpander, 13, PCA9535_MODE_OUTPUT}};
IOPinHandle_t BF_TP7 = {&PCA9535PinDriver, &(PCA9535Pin_t){&bristlefinIOExpander, 14, PCA9535_MODE_INPUT}};
IOPinHandle_t BF_TP8 = {&PCA9535PinDriver, &(PCA9535Pin_t){&bristlefinIOExpander, 15, PCA9535_MODE_INPUT}};