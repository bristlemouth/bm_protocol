
// Includes from CubeMX Generated files
#include "main.h"

// Peripheral
#include "main.h"
#include "gpdma.h"
#include "i2c.h"
#include "icache.h"
#include "iwdg.h"
#include "spi.h"
#include "usart.h"
#include "usb_otg.h"
#include "gpio.h"

// Includes for FreeRTOS
#include "FreeRTOS.h"
#include "task.h"

#include "bm_l2.h"
#include "bristlemouth.h"
#include "bootloader_helper.h"
#include "bsp.h"
#include "cli.h"
#include "debug.h"
#include "debug_adin_raw.h"
#include "debug_gpio.h"
#include "debug_i2c.h"
#include "debug_ina232.h"
#include "debug_memfault.h"
#include "debug_rtc.h"
#include "debug_spi.h"
#include "debug_sys.h"
#include "debug_uart.h"
#include "debug_w25.h"
#include "gpioISR.h"
#include "htu21d.h"
#include "ina232.h"
#include "io.h"
#include "lpm.h"
#include "memfault_platform_core.h"
#include "ms5803.h"
#include "pca9535.h"
#include "serial.h"
#include "serial_console.h"
#include "stm32_rtc.h"
#include "usb.h"
#include "w25.h"
#include "watchdog.h"
#include "debug_htu.h"
#ifdef USE_BOOTLOADER
#include "mcuboot_cli.h"
#endif

#include <stdio.h>

static void defaultTask(void *parameters);

#ifndef DEBUG_USE_USART1
SerialHandle_t usart1 = {
  .device = USART1,
  .name = "debug",
  .txPin = &DEBUG_TX,
  .rxPin = &DEBUG_RX,
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
#endif /// DEBUG_USE_USART1

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


static const DebugI2C_t debugI2CInterfaces[] = {
  {1, &i2c1}
};

static const DebugSPI_t debugSPIInterfaces[] = {
  {2, &spi2},
  {3, &spi3},
};

// TODO - move this to some debug file?
static const DebugGpio_t debugGpioPins[] = {
  {"adin_cs", &ADIN_CS, GPIO_OUT},
  {"adin_int", &ADIN_INT, GPIO_IN},
  {"adin_pwr", &ADIN_PWR, GPIO_OUT},
  {"bm_int", &BM_INT, GPIO_IN},
  {"bm_cs", &BM_CS, GPIO_OUT},
  {"flash_cs", &FLASH_CS, GPIO_OUT},
  {"boot", &BOOT, GPIO_IN},
  {"vusb_detect", &VUSB_DETECT, GPIO_IN},
  {"adin_rst", &ADIN_RST, GPIO_OUT},
  {"boost_en", &BOOST_EN, GPIO_OUT},
  {"vbus_sw_en", &VBUS_SW_EN, GPIO_OUT},
  {"led_g", &LED_G, GPIO_OUT},
  {"led_r", &LED_R, GPIO_OUT},
  {"tp10", &TP10, GPIO_OUT},
};

#ifndef DEBUG_USE_USART1
extern "C" void USART1_IRQHandler(void) {
  serialGenericUartIRQHandler(&usart1);
}
#endif // DEBUG_USE_USART1

static INA::INA232 debugIna1(&i2c1, I2C_INA_PODL_ADDR);
static INA::INA232 *debugIna[NUM_INA232_DEV] = {
  &debugIna1,
};

extern "C" int main(void) {

  // Before doing anything, check if we should enter ROM bootloader
  enterBootloaderIfNeeded();

  HAL_Init();

  SystemClock_Config();
  SystemPower_Config_ext();
  MX_GPIO_Init();
  MX_USART1_UART_Init();
  MX_USART3_UART_Init();
  MX_USB_OTG_FS_PCD_Init();
  MX_GPDMA1_Init();
  MX_ICACHE_Init();
  MX_IWDG_Init();

  usbMspInit();

  rtcInit();

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

extern SerialHandle_t usart3;

static void defaultTask( void *parameters ) {

  (void)parameters;

  startIWDGTask();
  startSerial();

  startCLI();
  pca9535StartIRQTask();

  // Use USB for serial console if USB for bringup
  startSerialConsole(&usbCLI);

  gpioISRStartTask();

  memfault_platform_boot();
  memfault_platform_start();

  bspInit();

  usbInit(&VUSB_DETECT, usb_is_connected);

  usart3.txPin = &BM_MOSI_TX3;
  usart3.rxPin = &BM_SCK_RX3;
  startDebugUart();
  spiflash::W25 debugW25(&spi2, &FLASH_CS);
  debugSysInit();
  debugAdinRawInit();
  debugGpioInit(debugGpioPins, sizeof(debugGpioPins)/sizeof(DebugGpio_t));
  debugI2CInit(debugI2CInterfaces, sizeof(debugI2CInterfaces)/sizeof(DebugI2C_t));
  debugSPIInit(debugSPIInterfaces, sizeof(debugSPIInterfaces)/sizeof(DebugSPI_t));
  debugW25Init(&debugW25);
  debugINA232Init(debugIna, NUM_INA232_DEV);
#ifndef DEBUG_USE_USART1
  debugMemfaultInit(&usart1);
#endif // DEBUG_USE_USART1
  debugRTCInit();
#ifdef USE_BOOTLOADER
  mcubootCliInit();
#endif

  // Commenting out while we test usart1
  // lpmPeripheralInactive(LPM_BOOT);

  while(1) {
    vTaskDelay(10000);
  }

}
