
// Includes from CubeMX Generated files
#include "main.h"

// Peripheral
#include "main.h"
#include "adc.h"
#include "rtc.h"
#include "icache.h"
#include "ucpd.h"
#include "usart.h"
#include "usb_otg.h"
#include "gpio.h"

// Includes for FreeRTOS
#include "FreeRTOS.h"
#include "task.h"

#include "serial.h"
#include "serial_console.h"
#include "cli.h"
#include "debug_memfault.h"

#include <stdio.h>

static void defaultTask(void *parameters);

// static void defaultTask( void *parameters );

SerialHandle_t usart1 = {
  .device = USART1,
  .name = "usart1",
  .txPin = NULL,
  .rxPin = NULL,
  .txStreamBuffer = NULL,
  .rxStreamBuffer = NULL,
  .txBufferSize = 1024,
  .rxBufferSize = 512,
  .rxBytesFromISR = serialGenericRxBytesFromISR,
  .getTxBytesFromISR = serialGenericGetTxBytesFromISR,
  .processByte = NULL,
  .data = NULL,
  .enabled = false,
  .flags = 0,
};

extern "C" void USART1_IRQHandler(void) {
  serialGenericUartIRQHandler(&usart1);
}

extern "C" int main(void) {

  // Before doing anything, check if we should enter ROM bootloader
  // enterBootloaderIfNeeded();

  HAL_Init();

  SystemClock_Config();

  SystemPower_Config_ext();
  MX_GPIO_Init();
  MX_ADC1_Init();
  MX_UCPD1_Init();
  MX_USART1_UART_Init();
  MX_USB_OTG_FS_PCD_Init();
  MX_ICACHE_Init();
  MX_RTC_Init();

  // usbMspInit();

  // Enable the watchdog timer
  // MX_IWDG_Init();

  // rtcInit();

  // Enable hardfault on divide-by-zero
  // SCB->CCR |= 0x10;


  // Initialize low power manager
  lpmInit();

  // Inhibit low power mode during boot process
  lpmPeripheralActive(LPM_BOOT);

  BaseType_t rval = xTaskCreate(
              defaultTask,
              "Default",
              // TODO - verify stack size
              128 * 4,
              NULL,
              // Start with very high priority during boot then downgrade
              // once done initializing everything
              2,
              NULL);
  configASSERT(rval == pdTRUE);

  // Start FreeRTOS scheduler
  vTaskStartScheduler();

  /* We should never get here as control is now taken by the scheduler */

  while (1){};
}


static void defaultTask( void *parameters ) {

  (void)parameters;

  startSerial();
  startSerialConsole(&usart1);
  startCLI();
  serialEnable(&usart1);

  debugMemfaultInit();

  // Commenting out while we test usart1
  // lpmPeripheralInactive(LPM_BOOT);

  // uint32_t count = 0;
  while(1) {
    HAL_GPIO_WritePin(LED_BLUE_GPIO_Port, LED_BLUE_Pin, (GPIO_PinState)1);
    vTaskDelay(2);
    HAL_GPIO_WritePin(LED_BLUE_GPIO_Port, LED_BLUE_Pin, (GPIO_PinState)0);
    vTaskDelay(998);
  }

}
