#include "bsp.h"
#include "FreeRTOS.h"
#include "task.h"

#include "i2c.h"
#include "io.h"
#include "main.h"
#include "pca9535.h"
#include "spi.h"
#include "stm32_io.h"
#include "tim.h"
// Peripheral includes for MX_ init functions
#include "bm_os.h"
#include "gpdma.h"
#include "gpio.h"
#include "icache.h"
#include "iwdg.h"
#include "usb_otg.h"

extern __IO uint32_t uwTick;
static bool osStarted = false;
static BmSemaphore delay_sem;

uint32_t HAL_GetTick(void) {
  if (osStarted) {
    return xTaskGetTickCount();
  } else {
    return uwTick;
  }
}

void HAL_Delay(uint32_t Delay) {
  if (osStarted) {
    vTaskDelay(Delay);
  } else {
    uint32_t tickstart = HAL_GetTick();
    uint32_t wait = Delay;

    /* Add a period to guaranty minimum wait */
    if (wait < HAL_MAX_DELAY) {
      wait += (uint32_t)uwTickFreq;
    }

    while ((HAL_GetTick() - tickstart) < wait) {
    };
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

  // Turn on power by default
  IOWrite(&POWER_EN, 1);
}

bool usb_is_connected() {
  uint8_t vusb = 0;

  IORead(&VUSB_DETECT, &vusb);

  return (bool)vusb;
}

extern "C" {
void tim2_cb(void) { bm_semaphore_give(delay_sem); }
}

void delay_us(uint64_t us) {
  __HAL_TIM_SET_COUNTER(&htim2, 0);
  __HAL_TIM_SET_AUTORELOAD(&htim2, us);

  // Clear interrupt flag if set before timer has been started
  if (__HAL_TIM_GET_FLAG(&htim2, TIM_FLAG_UPDATE)) {
    __HAL_TIM_CLEAR_FLAG(&htim2, TIM_FLAG_UPDATE);
    HAL_NVIC_ClearPendingIRQ(TIM2_IRQn);
  }

  HAL_TIM_Base_Start_IT(&htim2);
  bm_semaphore_take(delay_sem, portMAX_DELAY);
  HAL_TIM_Base_Stop_IT(&htim2);
}

void mxInit(void) {
  delay_sem = bm_semaphore_create();
  MX_GPIO_Init();
  MX_USB_OTG_FS_PCD_Init();
  MX_GPDMA1_Init();
  MX_ICACHE_Init();
  MX_IWDG_Init();
  MX_TIM5_Init();
  MX_TIM3_Init();
  MX_TIM2_Init();
}
