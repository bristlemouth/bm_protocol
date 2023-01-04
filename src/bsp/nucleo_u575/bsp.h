#pragma once

#define BSP_NAME "nucleo_u575"

#include "main.h"
#include "stm32u5xx.h"
#include "protected_spi.h"
#include "io.h"

#ifdef __cplusplus
extern "C" {
#endif

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

// SPI Interfaces
extern SPIInterface_t spi1;

#ifdef __cplusplus
}
#endif
