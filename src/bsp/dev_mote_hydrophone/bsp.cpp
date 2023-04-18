#include "bsp.h"
#include "FreeRTOS.h"
#include "task.h"

#include "adc.h"
#include "i2c.h"
#include "io.h"
#include "io_adc.h"
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

extern SPI_HandleTypeDef hspi1;
extern SPI_HandleTypeDef hspi2;
extern SPI_HandleTypeDef hspi3;
SPIInterface_t spi1 = PROTECTED_SPI("SPI1", hspi1, MX_SPI1_Init);
SPIInterface_t spi2 = PROTECTED_SPI("SPI2", hspi2, MX_SPI2_Init);
SPIInterface_t spi3 = PROTECTED_SPI("SPI3", hspi3, MX_SPI3_Init);

extern I2C_HandleTypeDef hi2c1;
I2CInterface_t i2c1 = PROTECTED_I2C("I2C1", hi2c1, MX_I2C1_Init);

PCA9535Device_t devMoteIOExpander = {&i2c1, 0x20, 0 , 0, 0, {NULL}, false, NULL};

adin_pins_t adin_pins = {&spi3, &ADIN_CS, &ADIN_INT, &ADIN_RST};

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

  // Initialize the IO Expander
  pca9535Init(&devMoteIOExpander);

  // Turn LEDS on by default
  IOWrite(&EXP_LED_G1, 0);
  IOWrite(&EXP_LED_R1, 0);
  IOWrite(&EXP_LED_G2, 0);
  IOWrite(&EXP_LED_R2, 0);

  IOConfigure(&EXP_LED_G1, NULL);
  IOConfigure(&EXP_LED_R1, NULL);
  IOConfigure(&EXP_LED_G2, NULL);
  IOConfigure(&EXP_LED_R2, NULL);
  IOConfigure(&VUSB_DETECT, NULL);
  IOConfigure(&ADIN_RST, NULL);
  IOConfigure(&EXP_GPIO3, NULL);
  IOConfigure(&EXP_GPIO4, NULL);
  IOConfigure(&EXP_GPIO5, NULL);
  IOConfigure(&EXP_GPIO6, NULL);
  IOConfigure(&EXP_GPIO7, NULL);
  IOConfigure(&EXP_GPIO8, NULL);
  IOConfigure(&EXP_GPIO9, NULL);
  IOConfigure(&EXP_GPIO10, NULL);
  IOConfigure(&EXP_GPIO11, NULL);
  IOConfigure(&EXP_GPIO12, NULL);
  IOConfigure(&EXP_INT, NULL);

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
  uint8_t vusb = 0;

  IORead(&VUSB_DETECT, &vusb);

  return (bool)vusb;
}
