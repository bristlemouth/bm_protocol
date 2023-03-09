#include "bsp.h"
#include "FreeRTOS.h"
#include "task.h"

#include "adc.h"
#include "io.h"
#include "io_adc.h"
#include "main.h"
#include "spi.h"
#include "i2c.h"
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

extern I2C_HandleTypeDef hi2c1;
extern SPI_HandleTypeDef hspi1;
SPIInterface_t spi1 = PROTECTED_SPI("SPI1", hspi1, MX_SPI1_Init);
I2CInterface_t i2c1 = PROTECTED_I2C("I2C1", hi2c1, MX_I2C1_Init);
PCA9535Device_t devMoteIOExpander = {&i2c2, 0x20, 0 , 0, 0, {NULL}, false};

void bspInit() {
  // Switch HAL_GetTick to use freeRTOS tick
  osStarted = true;
  HAL_SuspendTick();

  spiInit(&spi1);
  i2cInit(&i2c1);
}

// Helper function for sampling ADC on STM32
uint32_t adcGetSampleMv(uint32_t channel) {
  int32_t result = 0;

  ADC_ChannelConfTypeDef config = {};

  config.Rank = ADC_REGULAR_RANK_1;
  config.SamplingTime = ADC_SAMPLETIME_814CYCLES;
  config.SingleDiff = ADC_SINGLE_ENDED;
  config.OffsetNumber = ADC_OFFSET_NONE;
  config.Offset = 0;

  config.Channel = channel;
  IOAdcChannelConfig(&hadc1, &config);

  IOAdcReadMv(&hadc1, &result);
  return ((uint32_t)result);
}

bool usb_is_connected() {
  // TODO 
  return false;
}