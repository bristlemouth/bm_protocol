#pragma once

#define BSP_NAME "nucleo_u575"

#include "main.h"
#include "stm32u5xx.h"
#include "protected_spi.h"
#include "io.h"

#ifdef __cplusplus
extern "C" {
#endif

#define STM32_UUID ((uint32_t *)0x1FFF7A10)

#define VBUS_SENSE_CH ADC_CHANNEL_3

void bspInit();

// Pin definitions
extern IOPinHandle_t ADIN_CS;
extern IOPinHandle_t ADIN_RDY;
extern IOPinHandle_t ADIN_RST;
extern IOPinHandle_t LED_RED;
extern IOPinHandle_t LED_GREEN;
extern IOPinHandle_t LED_BLUE;
extern IOPinHandle_t USART1_TX;
extern IOPinHandle_t USART1_RX;
extern IOPinHandle_t USER_BUTTON;
extern IOPinHandle_t ALARM_OUT;
extern IOPinHandle_t FLASH_CS;

// SPI Interfaces
extern SPIInterface_t spi1;
extern SPIInterface_t spi2;

uint32_t adcGetSampleMv(uint32_t channel);
bool usb_is_connected();

typedef struct adin_pins_s {
	SPIInterface_t *spiInterface;
	IOPinHandle_t *chipSelect;
	IOPinHandle_t *interrupt;
	IOPinHandle_t *reset;
} adin_pins_t;

#ifdef __cplusplus
}
#endif
