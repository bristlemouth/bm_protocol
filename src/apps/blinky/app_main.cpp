
// Includes from CubeMX Generated files
#include "main.h"

// Peripheral
#include "main.h"
#include "adc.h"
#include "gpdma.h"
#include "i2c.h"
#include "icache.h"
#include "iwdg.h"
#include "rtc.h"
#include "sai.h"
#include "spi.h"
#include "ucpd.h"
#include "usart.h"
#include "usb_otg.h"
#include "gpio.h"

// Includes for FreeRTOS
#include "FreeRTOS.h"
#include "task.h"

#include "bsp.h"
#include "cli.h"
#include "debug_memfault.h"
#include "debug_sys.h"
#include "debug_mic.h"
#include "w25.h"
#include "debug_w25.h"
#include "gpioISR.h"
#include "io.h"
#include "memfault_platform_core.h"
#include "serial.h"
#include "serial_console.h"
#include "usb.h"
#include "watchdog.h"

#ifdef USE_BOOTLOADER
#include "mcuboot_cli.h"
#endif

// #include "lwip/init.h"

#include <stdio.h>

static void defaultTask(void *parameters);

// Serial console (when no usb present)
SerialHandle_t usart1 = {
  .device = USART1,
  .name = "usart1",
  .txPin = NULL,
  .rxPin = NULL,
  .txStreamBuffer = NULL,
  .rxStreamBuffer = NULL,
  .txBufferSize = 4096,
  .rxBufferSize = 512,
  .rxBytesFromISR = serialGenericRxBytesFromISR,
  .getTxBytesFromISR = serialGenericGetTxBytesFromISR,
  .processByte = NULL,
  .data = NULL,
  .enabled = false,
  .flags = 0,
};

// Serial console USB device
SerialHandle_t usbCLI   = {
  .device = (void *)0, // Using CDC 0
  .name = "vcp-cli",
  .txPin = NULL,
  .rxPin = NULL,
  .txStreamBuffer = NULL,
  .rxStreamBuffer = NULL,
  .txBufferSize = 1024,
  .rxBufferSize = 512,
  .rxBytesFromISR = NULL,
  .getTxBytesFromISR = NULL,
  .processByte = NULL,
  .data = NULL,
  .enabled = false,
  .flags = 0,
};

// "bristlemouth" USB serial - Use TBD
SerialHandle_t usbPcap   = {
  .device = (void *)1, // Using CDC 1
  .name = "vcp-bm",
  .txPin = NULL,
  .rxPin = NULL,
  .txStreamBuffer = NULL,
  .rxStreamBuffer = NULL,
  .txBufferSize = 2048,
  .rxBufferSize = 64,
  .rxBytesFromISR = NULL,
  .getTxBytesFromISR = NULL,
  .processByte = NULL,
  .data = NULL,
  .enabled = false,
  .flags = 0,
};


extern "C" void USART1_IRQHandler(void) {
  serialGenericUartIRQHandler(&usart1);
}

extern SAI_HandleTypeDef hsai_BlockA1;

extern "C" int main(void) {

  // Before doing anything, check if we should enter ROM bootloader
  // enterBootloaderIfNeeded();

  HAL_Init();

  SystemClock_Config();

  SystemPower_Config_ext();
  MX_GPIO_Init();
  MX_UCPD1_Init();
  MX_USART1_UART_Init();
  MX_USB_OTG_FS_PCD_Init();
  MX_ICACHE_Init();
  MX_RTC_Init();
  MX_IWDG_Init();
  MX_GPDMA1_Init();

  usbMspInit();

  // rtcInit();

  // Enable hardfault on divide-by-zero
  SCB->CCR |= 0x10;

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

bool buttonPress(const void *pinHandle, uint8_t value, void *args) {
  (void)pinHandle;
  (void)args;

  printf("Button press! %d\n", value);
  usb_is_connected();
  return false;
}

static void defaultTask( void *parameters ) {

  (void)parameters;

  startIWDGTask();
  startSerial();

  startCLI();

  // Use USB for serial console if USB is connected on boot
  // Otherwise use ST-Link serial port
  if(usb_is_connected()) {
    startSerialConsole(&usbCLI);
    // Serial device will be enabled automatically when console connects
    // so no explicit serialEnable is required
  } else {
    startSerialConsole(&usart1);
    serialEnable(&usart1);
  }

  gpioISRStartTask();

  memfault_platform_boot();
  memfault_platform_start();

  bspInit();

  usbInit(&VUSB_DETECT, usb_is_connected);
  spiflash::W25 w25(&spi2,&FLASH_CS);


  debugSysInit();
  debugMicInit(&hsai_BlockA1);
  debugMemfaultInit(&usart1);
  debugW25Init(&w25);

#ifdef USE_BOOTLOADER
  mcubootCliInit();
#endif

  // Commenting out while we test usart1
  // lpmPeripheralInactive(LPM_BOOT);

  gpioISRRegisterCallback(&USER_BUTTON, buttonPress);

  // lwip_init();

  // uint32_t count = 0;
  while(1) {
    IOWrite(&LED_BLUE, 1);
    vTaskDelay(2);
    IOWrite(&LED_BLUE, 0);
    vTaskDelay(998);
  }

}
