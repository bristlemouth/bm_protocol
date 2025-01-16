
// Includes from CubeMX Generated files
#include "main.h"

// Peripheral
#include "gpio.h"
#include "icache.h"
#include "iwdg.h"
// #include "rtc.h"
#include "usart.h"
#include "usb_otg.h"

// Includes for FreeRTOS
#include "FreeRTOS.h"
#include "task.h"
#include "task_priorities.h"

#include "app_pub_sub.h"
#include "app_util.h"
#include "bristlemouth_client.h"
#include "bsp.h"
#include "cli.h"
#include "config_cbor_map_service.h"
#include "debug_bm_service.h"
#include "debug_configuration.h"
#include "debug_dfu.h"
#include "debug_gpio.h"
#include "debug_memfault.h"
#include "debug_nvm_cli.h"
#include "debug_pluart_cli.h"
#include "debug_rtc.h"
#include "debug_spotter.h"
#include "debug_sys.h"
#include "debug_w25.h"
#include "external_flash_partitions.h"
#include "gpdma.h"
#include "gpioISR.h"
#include "l2.h"
#include "loadCellSampler.h"
#include "memfault_platform_core.h"
#include "ms5803.h"
#include "nvmPartition.h"
#include "pca9535.h"
#include "pcap.h"
#include "printf.h"
#include "pubsub.h"
#include "ram_partitions.h"
#include "sensorSampler.h"
#include "sensorWatchdog.h"
#include "sensors.h"
#include "serial.h"
#include "serial_console.h"
#include "stm32_rtc.h"
#include "stress.h"
#include "sys_info_service.h"
#include "timer_callback_handler.h"
#include "usb.h"
#include "w25.h"
#include "watchdog.h"
/* USER FILE INCLUDES */
#include "user_code.h"
/* USER FILE INCLUDES END */

#ifdef USE_MICROPYTHON
#include "micropython_freertos.h"
#endif

#include <stdio.h>
#include <string.h>

static void defaultTask(void *parameters);

// Serial console (when no usb present)
SerialHandle_t usart3 = {
    .device = USART3,
    .name = "usart3",
    .txPin = &BM_MOSI_TX3,
    .rxPin = &BM_SCK_RX3,
    .interruptPin = NULL,
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
    .preTxCb = NULL,
    .postTxCb = NULL,
};

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

NvmPartition *userConfigurationPartition = NULL;
NvmPartition *systemConfigurationPartition = NULL;
NvmPartition *hardwareConfigurationPartition = NULL;
NvmPartition *dfu_partition_global = NULL;

uint32_t sys_cfg_sensorsPollIntervalMs = DEFAULT_SENSORS_POLL_MS;
uint32_t sys_cfg_sensorsCheckIntervalS = DEFAULT_SENSORS_CHECK_S;

extern "C" void USART3_IRQHandler(void) { serialGenericUartIRQHandler(&usart3); }

// Only needed if we want the debug commands too
// extern INA::INA232 *debugIna;

extern "C" int main(void) {

  // Before doing anything, check if we should enter ROM bootloader
  // enterBootloaderIfNeeded();

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

bool start_stress(const void *pinHandle, uint8_t value, void *args) {
  (void)pinHandle;
  (void)args;
  static bool started;

  if (!started && value) {
    started = true;
    stress_start_tx((2 * 1024) / 8);
  }

  return false;
}

void handle_sensor_subscriptions(uint64_t node_id, const char *topic, uint16_t topic_len,
                                 const uint8_t *data, uint16_t data_len, uint8_t type,
                                 uint8_t version) {
  (void)node_id;
  (void)version;
  (void)type;
  if (strncmp("button", topic, topic_len) == 0) {
    if (strncmp("on", reinterpret_cast<const char *>(data), data_len) == 0) {
      IOWrite(&LED_GREEN, BB_LED_ON);
    } else if (strncmp("off", reinterpret_cast<const char *>(data), data_len) == 0) {
      IOWrite(&LED_GREEN, BB_LED_OFF);
    } else {
      // Not handled
    }
  } else {
    printf("Topic: %.*s\n", topic_len, topic);
    printf("Data: %.*s\n", data_len, data);
  }
}

void handle_bm_subscriptions(uint64_t node_id, const char *topic, uint16_t topic_len,
                             const uint8_t *data, uint16_t data_len, uint8_t type,
                             uint8_t version) {
  (void)node_id;
  if (strncmp(APP_PUB_SUB_UTC_TOPIC, topic, topic_len) == 0) {
    if (type == APP_PUB_SUB_UTC_TYPE && version == APP_PUB_SUB_UTC_VERSION) {
      utcDateTime_t time;
      const bm_common_pub_sub_utc_t *utc =
          reinterpret_cast<const bm_common_pub_sub_utc_t *>(data);
      dateTimeFromUtc(utc->utc_us, &time);

      RTCTimeAndDate_t rtc_time = {
          .year = time.year,
          .month = time.month,
          .day = time.day,
          .hour = time.hour,
          .minute = time.min,
          .second = time.sec,
          .ms = (time.usec / 1000),
      };

      if (rtcSet(&rtc_time) == pdPASS) {
        printf("Set RTC to %04u-%02u-%02uT%02u:%02u:%02u.%03u\n", rtc_time.year, rtc_time.month,
               rtc_time.day, rtc_time.hour, rtc_time.minute, rtc_time.second, rtc_time.ms);
      } else {
        printf("\n Failed to set RTC.\n");
      }
    } else {
      printf("Unrecognized version: %u and type: %u\n", version, type);
    }
  } else {
    printf("Topic: %.*s\n", topic_len, topic);
    printf("Data: %.*s\n", data_len, data);
  }
}

// TODO - move this to some debug file
// Defines lost if GPIOs for Debug CLI
static const DebugGpio_t debugGpioPins[] = {
    {"adin_cs", &ADIN_CS, GPIO_OUT},       {"adin_int", &ADIN_INT, GPIO_IN},
    {"adin_pwr", &ADIN_PWR, GPIO_OUT},     {"adin_rst", &ADIN_RST, GPIO_OUT},
    {"bb_3v3_en", &BB_3V3_EN, GPIO_OUT},   {"gpio1", &GPIO1, GPIO_OUT},
    {"led_green", &LED_GREEN, GPIO_OUT},   {"led_blue", &LED_BLUE, GPIO_OUT},
    {"led_red", &LED_RED, GPIO_OUT},       {"bb_pl_buck_en", &BB_PL_BUCK_EN, GPIO_OUT},
    {"bb_vbus_en", &BB_VBUS_EN, GPIO_OUT}, {"flash_cs", &FLASH_CS, GPIO_OUT},
    {"boot_led", &BOOT_LED, GPIO_IN},      {"vusb_detect", &VUSB_DETECT, GPIO_IN},
};

/* USER CODE EXECUTED HERE */
static void user_task(void *parameters);

void user_code_start() {
  BaseType_t rval = xTaskCreate(user_task, "USER", 4096, NULL, USER_TASK_PRIORITY, NULL);
  configASSERT(rval == pdPASS);
}

static void user_task(void *parameters) {
  (void)parameters;

  setup();

  for (;;) {
    loop();
    /*
      DO NOT REMOVE
      This vTaskDelay delay is REQUIRED for the FreeRTOS task scheduler
      to allow for lower priority tasks to be serviced.
      Keep this delay in the range of 10 to 100 ms.
    */
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
/* USER CODE EXECUTED HERE END */

static void defaultTask(void *parameters) {
  (void)parameters;

  mxInit();

  usbMspInit();

  rtcInit();

  // Initialize low power manager
  lpmInit();

  // Inhibit low power mode during boot process
  lpmPeripheralActive(LPM_BOOT);

  // If we ever include the SWWDG, we need to have the memfault
  // platform boot before starting the watchdog task
  memfault_platform_boot();

  startIWDGTask();
  startSerial();

  startSerialConsole(&usbCLI);
  // Serial device will be enabled automatically when console connects
  // so no explicit serialEnable is required
  pcapInit(&usbPcap);

  startCLI();

  gpioISRStartTask();

  memfault_platform_start();

  pca9535StartIRQTask();

  bspInit();
  usbInit(&VUSB_DETECT, usb_is_connected);

  debugSysInit();
  debugMemfaultInit(&usbCLI);

  debugGpioInit(debugGpioPins, sizeof(debugGpioPins) / sizeof(DebugGpio_t));
  debugSpotterInit();
  debugRTCInit();
  timer_callback_handler_init();
  spiflash::W25 debugW25(&spi2, &FLASH_CS);
  debugW25Init(&debugW25);
  NvmPartition debug_user_partition(debugW25, user_configuration);
  NvmPartition debug_hardware_partition(debugW25, hardware_configuration);
  NvmPartition debug_system_partition(debugW25, system_configuration);
  userConfigurationPartition = &debug_user_partition;
  systemConfigurationPartition = &debug_system_partition;
  hardwareConfigurationPartition = &debug_hardware_partition;
  NvmPartition debug_cli_partition(debugW25, cli_configuration);
  NvmPartition dfu_partition(debugW25, dfu_configuration);
  dfu_partition_global = &dfu_partition;
  debugNvmCliInit(&debug_cli_partition, &dfu_partition);
  debugPlUartCliInit();
  debugDfuInit(&dfu_partition);
  bcl_init();
  debugConfigurationInit();
  get_config_uint(BM_CFG_PARTITION_SYSTEM, "sensorsPollIntervalMs",
                  strlen("sensorsPollIntervalMs"), &sys_cfg_sensorsPollIntervalMs);
  get_config_uint(BM_CFG_PARTITION_SYSTEM, "sensorsCheckIntervalS",
                  strlen("sensorsCheckIntervalS"), &sys_cfg_sensorsCheckIntervalS);

  sensorConfig_t sensorConfig = {.sensorCheckIntervalS = sys_cfg_sensorsCheckIntervalS,
                                 .sensorsPollIntervalMs = sys_cfg_sensorsPollIntervalMs};
  sensorSamplerInit(&sensorConfig);
  // must call sensorsInit after sensorSamplerInit
  sensorsInit();
  debugBmServiceInit();
  sys_info_service_init();
  config_cbor_map_service_init();
  SensorWatchdog::SensorWatchdogInit();
  bm_sub(APP_PUB_SUB_UTC_TOPIC, handle_bm_subscriptions);

  // Turn of the bristleback leds
  IOWrite(&LED_GREEN, BB_LED_OFF);
  IOWrite(&LED_BLUE, BB_LED_OFF);
  IOWrite(&LED_RED, BB_LED_OFF);
  IOWrite(&BB_3V3_EN,
          1);                 // 1 enables, 0 disables. Needed for I2C and I/O control.
  IOWrite(&BB_VBUS_EN, 1);    // 0 enables, 1 disables. Needed for VOUT and 5V.
  IOWrite(&BB_PL_BUCK_EN, 1); // 0 enables, 1 disables. Vout
#ifdef USE_MICROPYTHON
  micropython_freertos_init(&usbCLI);
#endif
  user_code_start();

  // Re-enable low power mode
  // lpmPeripheralInactive(LPM_BOOT);

  while (1) {
    /* Do nothing */
    vTaskDelay(1000);
  }
}
