#pragma once

#define BSP_NAME "mote_bristleback_v1_0"

#include "io.h"
#include "main.h"
#include "protected_i2c.h"
#include "protected_spi.h"
#include "stm32u5xx.h"

#ifdef __cplusplus
extern "C" {
#endif

void mxInit(void);
void bspInit();
void delay_us(uint64_t us);

// Pin definitions
extern IOPinHandle_t IMU_INT;
extern IOPinHandle_t VUSB_DETECT;
extern IOPinHandle_t POWER_EN;
extern IOPinHandle_t MOTOR_SPEED1;
extern IOPinHandle_t BM_CS;
extern IOPinHandle_t BM_SCK;
extern IOPinHandle_t BM_MISO;
extern IOPinHandle_t BM_MOSI;
extern IOPinHandle_t MOTOR_SPEED2;
extern IOPinHandle_t VBUS_EN;
extern IOPinHandle_t BARO_INT;
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

#define I2C_INA_MAIN_ADDR (0x43)
#define I2C_INA_PODL_ADDR (0x41)
#define I2C_BMP581_ADDR (0x47)
#define NUM_INA232_DEV (2)

#ifdef __cplusplus
}
#endif
