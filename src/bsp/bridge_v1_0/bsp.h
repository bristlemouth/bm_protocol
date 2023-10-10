#pragma once

#define BSP_NAME "bridge_v1_0"

#include "main.h"
#include "stm32u5xx.h"
#include "protected_spi.h"
#include "protected_i2c.h"
#include "io.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DEBUG_USE_USART1
// #define DEBUG_USE_USART3

#define STM32_UUID ((uint32_t *)0x1FFF7A10)

#define VBUS_SENSE_CH ADC_CHANNEL_3

void mxInit(void);
void bspInit();

// Pin definitions
extern IOPinHandle_t ADIN_PWR;
extern IOPinHandle_t BM_CS;
extern IOPinHandle_t BM_SCK_RX3;
extern IOPinHandle_t BM_MISO;
extern IOPinHandle_t BM_MOSI_TX3;
extern IOPinHandle_t BM_INT;
extern IOPinHandle_t FLASH_SCK;
extern IOPinHandle_t FLASH_MISO;
extern IOPinHandle_t FLASH_MOSI;
extern IOPinHandle_t FLASH_CS;
extern IOPinHandle_t ADIN_CS;
extern IOPinHandle_t ADIN_SCK;
extern IOPinHandle_t ADIN_MISO;
extern IOPinHandle_t ADIN_MOSI;
extern IOPinHandle_t ADIN_RST;
extern IOPinHandle_t BOOT;
extern IOPinHandle_t ADIN_INT;
extern IOPinHandle_t DEBUG_RX;
extern IOPinHandle_t DEBUG_TX;
extern IOPinHandle_t BOOST_EN;
extern IOPinHandle_t VBUS_SW_EN;
extern IOPinHandle_t LED_G;
extern IOPinHandle_t LED_R;
extern IOPinHandle_t TP10;
extern IOPinHandle_t VUSB_DETECT;

// SPI Interfaces
extern SPIInterface_t spi2;
extern SPIInterface_t spi3;

// I2C Interfaces
extern I2CInterface_t i2c1;

uint32_t adcGetSampleMv(uint32_t channel);
bool usb_is_connected();

typedef struct adin_pins_s {
	SPIInterface_t *spiInterface;
	IOPinHandle_t *chipSelect;
	IOPinHandle_t *interrupt;
	IOPinHandle_t *reset;
} adin_pins_t;

#define I2C_INA_PODL_ADDR  (0x41)
#define NUM_INA232_DEV (1)

#ifdef __cplusplus
}
#endif
