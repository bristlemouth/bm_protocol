#include "bsp.h"
#include "FreeRTOS.h"
#include "task.h"

#include "i2c.h"
#include "io.h"
#include "main.h"
#include "pca9535.h"
#include "spi.h"
#include "stm32_io.h"
// Peripheral includes for MX_ init functions
#include "adc.h"
#include "gpdma.h"
#include "gpio.h"
#include "icache.h"
#include "iwdg.h"
#include "usart.h"
#include "usb_otg.h"

extern __IO uint32_t uwTick;
static bool osStarted = false;

uint32_t HAL_GetTick(void) {
  if(osStarted) {
    return xTaskGetTickCount();
  } else {
    return uwTick;
  }
}

void HAL_Delay(uint32_t Delay) {
  if(osStarted) {
    vTaskDelay(Delay);
  } else {
    uint32_t tickstart = HAL_GetTick();
    uint32_t wait = Delay;

    /* Add a period to guaranty minimum wait */
    if (wait < HAL_MAX_DELAY) {
      wait += (uint32_t)uwTickFreq;
    }

    while ((HAL_GetTick() - tickstart) < wait) {};
  }
}

extern SPI_HandleTypeDef hspi1;
extern SPI_HandleTypeDef hspi2;
extern SPI_HandleTypeDef hspi3;
SPIInterface_t spi1 = PROTECTED_SPI("SPI1", hspi1, MX_SPI1_Init, LPM_SPI1);
SPIInterface_t spi2 = PROTECTED_SPI("SPI2", hspi2, MX_SPI2_Init, LPM_SPI2);
SPIInterface_t spi3 = PROTECTED_SPI("SPI3", hspi3, MX_SPI3_Init, LPM_SPI3);

extern I2C_HandleTypeDef hi2c1;
I2CInterface_t i2c1 = PROTECTED_I2C("I2C1", hi2c1, MX_I2C1_Init, LPM_I2C1);

PCA9535Device_t bristlefinIOExpander = {&i2c1, 0x21, 0 , 0, 0, {NULL}, false, NULL};

adin_pins_t adin_pins = {&spi3, &ADIN_CS, &ADIN_INT, &ADIN_RST};

// Startup Pin Definitions: https://www.notion.so/sofarocean/Default-GPIO-states-5729cc41a5e541aa99ca954db8e76e2e
void bspInit() {
  // Switch HAL_GetTick to use freeRTOS tick
  osStarted = true;
  HAL_SuspendTick();

  spiInit(&spi1);
  spiInit(&spi2);
  spiInit(&spi3);
  i2cInit(&i2c1);

  // Turn on Adin2111
  IOWrite(&ADIN_PWR, 1);

  IOWrite(&I2C_MUX_RESET, 1);

  // Initialize the IO Expander
  if (pca9535Init(&bristlefinIOExpander) == pdPASS) {
    IORegisterCallback(&IOEXP_INT, pca9535IRQHandler, &bristlefinIOExpander);
  }

  // Turn LEDS on by default
  IOWrite(&BF_LED_G1, 0);
  IOWrite(&BF_LED_R1, 0);
  IOWrite(&BF_LED_G2, 0);
  IOWrite(&BF_LED_R2, 0);

  IOConfigure(&BF_IO1, NULL);
  IOConfigure(&BF_IO2, NULL);
  IOConfigure(&BF_HFIO, NULL);
  IOConfigure(&BF_3V3_EN, NULL);
  IOConfigure(&BF_5V_EN, NULL);
  IOConfigure(&BF_IMU_INT, NULL);
  IOConfigure(&BF_IMU_RST, NULL);
  IOConfigure(&BF_SDI12_OE, NULL);
  IOConfigure(&BF_TP16, NULL);
  IOConfigure(&BF_PL_BUCK_EN, NULL);
  IOConfigure(&BF_TP7, NULL);
  IOConfigure(&BF_TP8, NULL);
  IOConfigure(&BF_LED_G1, NULL);
  IOConfigure(&BF_LED_R1, NULL);
  IOConfigure(&BF_LED_G2, NULL);
  IOConfigure(&BF_LED_R2, NULL);
}

bool usb_is_connected() {
  uint8_t vusb = 0;

  IORead(&VUSB_DETECT, &vusb);

  return (bool)vusb;
}

void mxInit(void) {
  MX_GPIO_Init();
  MX_USB_OTG_FS_PCD_Init();
  MX_GPDMA1_Init();
  MX_ICACHE_Init();
  MX_IWDG_Init();
  MX_ADC1_Init();
}