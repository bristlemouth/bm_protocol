#include "bsp.h"
#include "FreeRTOS.h"
#include "task.h"

#include "main.h"
#include "spi.h"
#include "stm32_io.h"

// Define all GPIO's here
IOPinHandle_t ADIN_CS = {&STM32PinDriver, &(STM32Pin_t){ADIN_CS_GPIO_Port, ADIN_CS_Pin, NULL, NULL}};
IOPinHandle_t LED_RED = {&STM32PinDriver, &(STM32Pin_t){LED_RED_GPIO_Port, LED_RED_Pin, NULL, NULL}};
IOPinHandle_t LED_GREEN = {&STM32PinDriver, &(STM32Pin_t){LED_GREEN_GPIO_Port, LED_GREEN_Pin, NULL, NULL}};
IOPinHandle_t LED_BLUE = {&STM32PinDriver, &(STM32Pin_t){LED_BLUE_GPIO_Port, LED_BLUE_Pin, NULL, NULL}};
IOPinHandle_t USART1_TX = {&STM32PinDriver, &(STM32Pin_t){USART1_TX_GPIO_Port, USART1_TX_Pin, NULL, NULL}};
IOPinHandle_t USART1_RX = {&STM32PinDriver, &(STM32Pin_t){USART1_RX_GPIO_Port, USART1_RX_Pin, NULL, NULL}};


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