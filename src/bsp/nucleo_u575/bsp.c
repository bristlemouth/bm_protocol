#include "bsp.h"
#include "FreeRTOS.h"
#include "task.h"

#include "adc.h"
#include "io.h"
#include "io_adc.h"
#include "main.h"
#include "spi.h"
#include "stm32_io.h"

// Define all GPIO's here
IOPinHandle_t ADIN_CS = {&STM32PinDriver, &(STM32Pin_t){ADIN_CS_GPIO_Port, ADIN_CS_Pin, NULL, NULL}};
IOPinHandle_t ADIN_RDY = {&STM32PinDriver, &(STM32Pin_t){ADIN_RDY_GPIO_Port, ADIN_RDY_Pin, NULL, NULL}};
IOPinHandle_t ADIN_RST = {&STM32PinDriver, &(STM32Pin_t){ADIN_RST_GPIO_Port, ADIN_RST_Pin, NULL, NULL}};
IOPinHandle_t LED_RED = {&STM32PinDriver, &(STM32Pin_t){LED_RED_GPIO_Port, LED_RED_Pin, NULL, NULL}};
IOPinHandle_t LED_GREEN = {&STM32PinDriver, &(STM32Pin_t){LED_GREEN_GPIO_Port, LED_GREEN_Pin, NULL, NULL}};
IOPinHandle_t LED_BLUE = {&STM32PinDriver, &(STM32Pin_t){LED_BLUE_GPIO_Port, LED_BLUE_Pin, NULL, NULL}};
IOPinHandle_t USART1_TX = {&STM32PinDriver, &(STM32Pin_t){USART1_TX_GPIO_Port, USART1_TX_Pin, NULL, NULL}};
IOPinHandle_t USART1_RX = {&STM32PinDriver, &(STM32Pin_t){USART1_RX_GPIO_Port, USART1_RX_Pin, NULL, NULL}};
IOPinHandle_t USER_BUTTON = {&STM32PinDriver, &(STM32Pin_t){USER_BUTTON_GPIO_Port, USER_BUTTON_Pin, NULL, NULL}};
IOPinHandle_t ALARM_OUT = {&STM32PinDriver, &(STM32Pin_t){ALARM_OUT_GPIO_Port, ALARM_OUT_Pin, NULL, NULL}};

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
SPIInterface_t spi1 = PROTECTED_SPI("SPI1", hspi1, MX_SPI1_Init);

void bspInit() {
  // Switch HAL_GetTick to use freeRTOS tick
  osStarted = true;
  HAL_SuspendTick();

  spiInit(&spi1);
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
  IOAdcInit(&hadc1);
  const float vbus_raw = (float)adcGetSampleMv(VBUS_SENSE_CH)/1000.0;

  // NUCLEO-U5 board has a 330kOhm | 49.9kOhm resistor divider on the
  // VBUS_SENSE signal.
  const float vbus = vbus_raw * (330.0 + 49.9)/49.9;
  IOAdcDeInit(&hadc1);

  return vbus > 4.0;
}
