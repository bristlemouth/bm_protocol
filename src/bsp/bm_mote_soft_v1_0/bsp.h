#pragma once

#define BSP_NAME "mote_v1_0"

#include "main.h"
#include "stm32u5xx.h"
#include "protected_spi.h"
#include "protected_i2c.h"
#include "io.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DEBUG_USE_LPUART1
#define DEBUG_USE_USART3

#define STM32_UUID ((uint32_t *)0x1FFF7A10)

void mxInit(void);
void bspInit();

// Pin definitions
extern IOPinHandle_t GPIO2;
extern IOPinHandle_t GPIO1;
extern IOPinHandle_t VUSB_DETECT;
extern IOPinHandle_t IOEXP_INT;
extern IOPinHandle_t I2C_MUX_RESET;
extern IOPinHandle_t BM_CS;
extern IOPinHandle_t BM_SCK_RX3;
extern IOPinHandle_t BM_MISO;
extern IOPinHandle_t BM_MOSI_TX3;
extern IOPinHandle_t BM_INT;
extern IOPinHandle_t VBUS_BF_EN;
extern IOPinHandle_t FLASH_SCK;
extern IOPinHandle_t FLASH_MISO;
extern IOPinHandle_t FLASH_MOSI;
extern IOPinHandle_t FLASH_CS;
extern IOPinHandle_t ADIN_CS;
extern IOPinHandle_t ADIN_PWR;
extern IOPinHandle_t ADIN_RST;
extern IOPinHandle_t ADIN_SCK;
extern IOPinHandle_t ADIN_MISO;
extern IOPinHandle_t ADIN_MOSI;
extern IOPinHandle_t BOOT_LED;
extern IOPinHandle_t ADIN_INT;
extern IOPinHandle_t DEBUG_RX;
extern IOPinHandle_t DEBUG_TX;
extern IOPinHandle_t PAYLOAD_RX;
extern IOPinHandle_t PAYLOAD_TX;

// Bristlefine IO expander pins
extern IOPinHandle_t BF_IO1;
extern IOPinHandle_t BF_IO2;
extern IOPinHandle_t BF_HFIO;
extern IOPinHandle_t BF_3V3_EN;
extern IOPinHandle_t BF_5V_EN;
extern IOPinHandle_t BF_IMU_INT;
extern IOPinHandle_t BF_IMU_RST;
extern IOPinHandle_t BF_SDI12_OE;
extern IOPinHandle_t BF_TP16;
extern IOPinHandle_t BF_LED_G1;
extern IOPinHandle_t BF_LED_R1;
extern IOPinHandle_t BF_LED_G2;
extern IOPinHandle_t BF_LED_R2;
extern IOPinHandle_t BF_PL_BUCK_EN;
extern IOPinHandle_t BF_TP7;
extern IOPinHandle_t BF_TP8;

// SPI Interfaces
extern SPIInterface_t spi1;
extern SPIInterface_t spi2;
extern SPIInterface_t spi3;

// I2C Interfaces
extern I2CInterface_t i2c1;

bool usb_is_connected();

typedef struct adin_pins_s {
	SPIInterface_t *spiInterface;
	IOPinHandle_t *chipSelect;
	IOPinHandle_t *interrupt;
	IOPinHandle_t *reset;
} adin_pins_t;

#define I2C_INA_MAIN_ADDR  (0x43)
#define I2C_INA_PODL_ADDR  (0x41)
#define NUM_INA232_DEV (2)

#define TCA9546A_ADDR (0x70)

#define MS5803_ADDR (0x77)

#ifdef __cplusplus
}
#endif
