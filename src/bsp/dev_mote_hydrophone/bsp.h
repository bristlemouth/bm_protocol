#pragma once

#define BSP_NAME "dev_mote_v1_0"

#include "main.h"
#include "stm32u5xx.h"
#include "protected_spi.h"
#include "protected_i2c.h"
#include "io.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DEBUG_USE_USART1
#define DEBUG_USE_LPUART1

#define STM32_UUID ((uint32_t *)0x1FFF7A10)

#define VBUS_SENSE_CH ADC_CHANNEL_3

void bspInit();

// Pin definitions
extern IOPinHandle_t GPIO2;
extern IOPinHandle_t GPIO1;
extern IOPinHandle_t ADIN_PWR;
extern IOPinHandle_t BM_CS;
extern IOPinHandle_t BM_SCK_RX3;
extern IOPinHandle_t BM_MISO;
extern IOPinHandle_t BM_MOSI_TX3;
extern IOPinHandle_t BM_INT;
extern IOPinHandle_t FLASH_SCK;
extern IOPinHandle_t FLASH_MISO;
extern IOPinHandle_t FLASH_MOSI;
extern IOPinHandle_t SAI1_SCK_A;
extern IOPinHandle_t ADIN_CS;
extern IOPinHandle_t ADIN_SCK;
extern IOPinHandle_t ADIN_MISO;
extern IOPinHandle_t ADIN_MOSI;
extern IOPinHandle_t BOOT_LED;
extern IOPinHandle_t ADIN_INT;
extern IOPinHandle_t SAI1_FS_A;
extern IOPinHandle_t SAI1_SD_A;
extern IOPinHandle_t PAYLOAD_RX;
extern IOPinHandle_t PAYLOAD_TX;

extern IOPinHandle_t EXP_LED_G1;
extern IOPinHandle_t EXP_LED_R1;
extern IOPinHandle_t EXP_LED_G2;
extern IOPinHandle_t EXP_LED_R2;
extern IOPinHandle_t VUSB_DETECT;
extern IOPinHandle_t ADIN_RST;
extern IOPinHandle_t EXP_GPIO3;
extern IOPinHandle_t EXP_GPIO4;
extern IOPinHandle_t EXP_GPIO5;
extern IOPinHandle_t EXP_GPIO6;
extern IOPinHandle_t EXP_GPIO7;
extern IOPinHandle_t EXP_GPIO8;
extern IOPinHandle_t EXP_GPIO9;
extern IOPinHandle_t EXP_GPIO10;
extern IOPinHandle_t EXP_GPIO11;
extern IOPinHandle_t EXP_GPIO12;
extern IOPinHandle_t EXP_INT;

// SPI Interfaces
extern SPIInterface_t spi1;
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

#define I2C_IN_MAIN_ADDR  (0x42)
#define I2C_INA_PODL_ADDR  (0x41)
#define NUM_INA232_DEV (2)

#ifdef __cplusplus
}
#endif
