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
extern IOPinHandle_t FLASH_CS;
extern IOPinHandle_t ADIN_NSS;
extern IOPinHandle_t ADIN_SCK;
extern IOPinHandle_t ADIN_MISO;
extern IOPinHandle_t ADIN_MOSI;
extern IOPinHandle_t BOOT_LED;
extern IOPinHandle_t ADIN_INT;

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

// SPI Interfaces
extern SPIInterface_t spi1;
extern I2CInterface_t i2c1;

uint32_t adcGetSampleMv(uint32_t channel);
bool usb_is_connected();

#ifdef __cplusplus
}
#endif