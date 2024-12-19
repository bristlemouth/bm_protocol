
// Includes from CubeMX Generated files
#include "main.h"

// Peripheral
#include "main.h"
// #include "adc.h"
#include "gpdma.h"
#include "gpio.h"
#include "i2c.h"
#include "icache.h"
#include "iwdg.h"
#include "spi.h"
#include "usart.h"
#include "usb_otg.h"

// Includes for FreeRTOS
#include "FreeRTOS.h"
#include "task.h"

#include "bme280driver.h"
#include "bootloader_helper.h"
#include "bristlefin.h"
#include "bristlemouth_client.h"
#include "bsp.h"
#include "cli.h"
#include "debug.h"
#include "debug_adin_raw.h"
#include "debug_configuration.h"
#include "debug_gpio.h"
#include "debug_htu.h"
#include "debug_i2c.h"
#include "debug_ina232.h"
#include "debug_memfault.h"
#include "debug_nvm_cli.h"
#include "debug_pressure_sensor.h"
#include "debug_rtc.h"
#include "debug_spi.h"
#include "debug_sys.h"
#include "debug_tca9546a.h"
#include "debug_uart.h"
#include "debug_w25.h"
#include "gpioISR.h"
#include "htu21d.h"
#include "ina232.h"
#include "io.h"
#include "l2.h"
#include "lpm.h"
#include "memfault_platform_core.h"
#include "ms5803.h"
#include "nvmPartition.h"
#include "pca9535.h"
#include "ram_partitions.h"
#include "serial.h"
#include "serial_console.h"
#include "stm32_rtc.h"
#include "tca9546a.h"
#include "timer_callback_handler.h"
#include "usb.h"
#include "w25.h"
#include "watchdog.h"
#ifdef USE_BOOTLOADER
#include "mcuboot_cli.h"
#endif

#include <stdio.h>

NvmPartition *userConfigurationPartition = NULL;
NvmPartition *systemConfigurationPartition = NULL;
NvmPartition *hardwareConfigurationPartition = NULL;

static void defaultTask(void *parameters);
#ifndef DEBUG_USE_LPUART1
SerialHandle_t lpuart1 = {
    .device = LPUART1,
    .name = "payload",
    .txPin = &PAYLOAD_TX,
    .rxPin = &PAYLOAD_RX,
    .interruptPin = NULL,
    .txStreamBuffer = NULL,
    .rxStreamBuffer = NULL,
    .txBufferSize = 128,
    .rxBufferSize = 2048,
    .rxBytesFromISR = serialGenericRxBytesFromISR,
    .getTxBytesFromISR = serialGenericGetTxBytesFromISR,
    .processByte = NULL,
    .data = NULL,
    .enabled = false,
    .flags = 0,
    .preTxCb = NULL,
    .postTxCb = NULL,
};
#endif // DEBUG_USE_LPUART1

#ifndef DEBUG_USE_USART3
SerialHandle_t usart3 = {
    .device = USART3,
    .name = "BM",
    .txPin = &BM_MOSI_TX3,
    .rxPin = &BM_SCK_RX3,
    .interruptPin = NULL,
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
    .preTxCb = NULL,
    .postTxCb = NULL,
};
#endif /// DEBUG_USE_USART3

// Serial console USB device
SerialHandle_t usbCLI = {
    .device = (void *)0, // Using CDC 0
    .name = "vcp-cli",
    .txPin = NULL,
    .rxPin = NULL,
    .interruptPin = NULL,
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
    .preTxCb = NULL,
    .postTxCb = NULL,
};

// "bristlemouth" USB serial - Use TBD
SerialHandle_t usbPcap = {
    .device = (void *)1, // Using CDC 1
    .name = "vcp-bm",
    .txPin = NULL,
    .rxPin = NULL,
    .interruptPin = NULL,
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
    .preTxCb = NULL,
    .postTxCb = NULL,
};

static const DebugI2C_t debugI2CInterfaces[] = {{1, &i2c1}};

static const DebugSPI_t debugSPIInterfaces[] = {
    {2, &spi2},
    {3, &spi3},
};

// TODO - move this to some debug file?
static const DebugGpio_t debugGpioPins[] = {
    {"adin_cs", &ADIN_CS, GPIO_OUT},
    {"adin_int", &ADIN_INT, GPIO_IN},
    {"adin_pwr", &ADIN_PWR, GPIO_OUT},
    {"adin_rst", &ADIN_RST, GPIO_OUT},
    {"i2c_mux_rst", &I2C_MUX_RESET, GPIO_OUT},
    {"gpio1", &GPIO1, GPIO_OUT},
    {"gpio2", &GPIO2, GPIO_OUT},
    {"bm_int", &BM_INT, GPIO_IN},
    {"ioex_int", &IOEXP_INT, GPIO_IN},
    {"bm_cs", &BM_CS, GPIO_OUT},
    {"vbus_bf_en", &VBUS_BF_EN, GPIO_OUT},
    {"flash_cs", &FLASH_CS, GPIO_OUT},
    {"boot_led", &BOOT_LED, GPIO_IN},
    {"vusb_detect", &VUSB_DETECT, GPIO_IN},
    {"bf_io1", &BF_IO1, GPIO_IN},
    {"bf_io2", &BF_IO2, GPIO_IN},
    {"bf_hfio", &BF_HFIO, GPIO_OUT},
    {"bf_3v3_en", &BF_3V3_EN, GPIO_OUT},
    {"bf_5v_en", &BF_5V_EN, GPIO_OUT},
    {"bf_imu_int", &BF_IMU_INT, GPIO_IN},
    {"bf_imu_rst", &BF_IMU_RST, GPIO_OUT},
    {"bf_sdi12_oe", &BF_SDI12_OE, GPIO_OUT},
    {"bf_tp16", &BF_TP16, GPIO_IN},
    {"bf_led_g1", &BF_LED_G1, GPIO_OUT},
    {"bf_led_r1", &BF_LED_R1, GPIO_OUT},
    {"bf_led_g2", &BF_LED_G2, GPIO_OUT},
    {"bf_led_r2", &BF_LED_R2, GPIO_OUT},
    {"bf_pl_buck_en", &BF_PL_BUCK_EN, GPIO_OUT},
    {"bf_tp7", &BF_TP7, GPIO_IN},
    {"bf_tp8", &BF_TP8, GPIO_IN},
};

#ifndef DEBUG_USE_LPUART1
extern "C" void LPUART1_IRQHandler(void) { serialGenericUartIRQHandler(&lpuart1); }
#endif // DEBUG_USE_LPUART1

static TCA::TCA9546A bristlefinTCA(&i2c1, TCA9546A_ADDR, &I2C_MUX_RESET);

static INA::INA232 debugIna1(&i2c1, I2C_INA_MAIN_ADDR);
static INA::INA232 debugIna2(&i2c1, I2C_INA_PODL_ADDR);
static INA::INA232 *debugIna[NUM_INA232_DEV] = {
    &debugIna1,
    &debugIna2,
};

static MS5803 debugPressure(&i2c1, MS5803_ADDR);
static Bme280 debugPHTU(&i2c1, Bme280::I2C_ADDR);
static HTU21D debugHTU(&i2c1);
static Bristlefin bristlefin(debugPressure, debugHTU, debugPHTU, bristlefinTCA, debugIna2);

extern "C" int main(void) {

  // Before doing anything, check if we should enter ROM bootloader
  enterBootloaderIfNeeded();

  HAL_Init();

  SystemClock_Config();
  SystemPower_Config_ext();

  // If you NEED to have an interrupt based timer, or other interrupts running before the
  // scheduler starts, you can enable them here. The reason for this is that FreeRTOS will
  // disable interrupts when calling FreeRTOS API functions before the scheduler starts.
  // In our case, this is done in some class constructors that utilize pvPortMalloc,
  // or other FreeRTOS API calls. This means that when __libc_init_array is called,
  // interrupts are disabled, and the timer interrupt will no longer be available until
  // the scheduler starts. This is a problem if you are initializing a peripheral that
  // includes a delay, see MX_USB_OTG_FS_PCD_Init() for an example where HAL_Delay()
  // is called. It is highly recommended to avoid this by initializing everything in the
  // default task. See https://www.freertos.org/FreeRTOS_Support_Forum_Archive/March_2017/freertos_What_is_normal_method_for_running_initialization_code_in_FreerTOS_92042073j.html
  // for more details.
  // portENABLE_INTERRUPTS();

  // Enable hardfault on divide-by-zero
  SCB->CCR |= 0x10;

  BaseType_t rval = xTaskCreate(defaultTask, "Default",
                                // TODO - verify stack size
                                128 * 4, NULL,
                                // Start with very high priority during boot then downgrade
                                // once done initializing everything
                                2, NULL);
  configASSERT(rval == pdTRUE);

  // Start FreeRTOS scheduler
  vTaskStartScheduler();

  /* We should never get here as control is now taken by the scheduler */

  while (1) {
  };
}

static void initSensors() {
  static constexpr uint8_t PHTU_INIT_MAX_RETRIES = 5;
  uint8_t try_count = 0;
  I2CResponse_t resp = debugPHTU.probe();
  while (resp != I2C_OK && try_count++ < PHTU_INIT_MAX_RETRIES) {
    vTaskDelay(10);
    resp = debugPHTU.probe();
  }
  if (resp == I2C_OK) {
    try_count = 0;
    while (!debugPHTU.init() && try_count++ < PHTU_INIT_MAX_RETRIES) {
      vTaskDelay(10);
    }
    if (try_count++ < PHTU_INIT_MAX_RETRIES) {
      debugHtuInit(&debugPHTU);
      debugPressureSensorInit(&debugPHTU);
    } else {
      printf("Failed to Initialize PHTU\n");
    }
  } else {
    try_count = 0;
    while (!debugPressure.init() && !debugHTU.init() && try_count++ < PHTU_INIT_MAX_RETRIES) {
      vTaskDelay(10);
    }
    if (try_count++ < PHTU_INIT_MAX_RETRIES) {
      debugHtuInit(&debugHTU);
      debugPressureSensorInit(&debugPressure);
    } else {
      printf("Failed to Initialize PHTU\n");
    }
  }
}

static void defaultTask(void *parameters) {

  (void)parameters;

  MX_GPIO_Init();
  MX_LPUART1_UART_Init();
  MX_USART3_UART_Init();
  MX_USB_OTG_FS_PCD_Init();
  MX_GPDMA1_Init();
  MX_ICACHE_Init();
  MX_IWDG_Init();

  usbMspInit();

  rtcInit();

  // Initialize low power manager
  lpmInit();

  // Inhibit low power mode during boot process
  lpmPeripheralActive(LPM_BOOT);

  startIWDGTask();
  startSerial();

  startCLI();
  startDebugUart();
  pca9535StartIRQTask();

  // Use USB for serial console if USB for bringup
  startSerialConsole(&usbCLI);

  gpioISRStartTask();

  memfault_platform_boot();
  memfault_platform_start();

  bspInit();

  usbInit(&VUSB_DETECT, usb_is_connected);

  bristlefin.setGpioDefault();
  if (bristlefinTCA.init()) {
    bristlefinTCA.setChannel(TCA::CH_1);
  }

  debugTCA9546AInit(&bristlefinTCA);
  timer_callback_handler_init();
  spiflash::W25 debugW25(&spi2, &FLASH_CS);
  debugSysInit();
  debugAdinRawInit();
  debugGpioInit(debugGpioPins, sizeof(debugGpioPins) / sizeof(DebugGpio_t));
  debugI2CInit(debugI2CInterfaces, sizeof(debugI2CInterfaces) / sizeof(DebugI2C_t));
  debugSPIInit(debugSPIInterfaces, sizeof(debugSPIInterfaces) / sizeof(DebugSPI_t));
  debugW25Init(&debugW25);
  debugINA232Init(debugIna, NUM_INA232_DEV);
  debugMemfaultInit(&usbCLI);
  initSensors();
  NvmPartition debug_user_partition(debugW25, user_configuration);
  NvmPartition debug_hardware_partition(debugW25, hardware_configuration);
  NvmPartition debug_system_partition(debugW25, system_configuration);
  userConfigurationPartition = &debug_user_partition;
  systemConfigurationPartition = &debug_system_partition;
  hardwareConfigurationPartition = &debug_hardware_partition;
  debugConfigurationInit();

  NvmPartition debug_cli_partition(debugW25, cli_configuration);
  NvmPartition dfu_cli_partition(debugW25, dfu_configuration);
  debugNvmCliInit(&debug_cli_partition, &dfu_cli_partition);
#ifdef USE_BOOTLOADER
  mcubootCliInit();
#endif
  debugRTCInit();

  // Commenting out while we test usart1
  // lpmPeripheralInactive(LPM_BOOT);

  while (1) {
    vTaskDelay(1000);
  }
}
