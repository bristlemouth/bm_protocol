#include "bsp.h"
#include "FreeRTOS.h"
#include "task.h"

#include "i2c.h"
#include "io.h"
#include "main.h"
#include "pca9535.h"
#include "spi.h"
#include "stm32_io.h"

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

extern SPI_HandleTypeDef hspi2;
extern SPI_HandleTypeDef hspi3;
SPIInterface_t spi2 = PROTECTED_SPI("SPI2", hspi2, MX_SPI2_Init);
SPIInterface_t spi3 = PROTECTED_SPI("SPI3", hspi3, MX_SPI3_Init);

extern I2C_HandleTypeDef hi2c1;
I2CInterface_t i2c1 = PROTECTED_I2C("I2C1", hi2c1, MX_I2C1_Init);

adin_pins_t adin_pins = {&spi3, &ADIN_CS, &ADIN_INT, &ADIN_RST};

void bspInit() {
  // Switch HAL_GetTick to use freeRTOS tick
  osStarted = true;
  HAL_SuspendTick();

  spiInit(&spi2, LPM_SPI2);
  spiInit(&spi3, LPM_SPI3);

  i2cInit(&i2c1,LPM_I2C1);

  // Turn on Adin2111
  IOWrite(&ADIN_PWR, 1);

}

bool usb_is_connected() {
  uint8_t vusb = 0;

  IORead(&VUSB_DETECT, &vusb);

  return (bool)vusb;
}
