
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

#include "app_config.h"
#include "app_pub_sub.h"
#include "app_util.h"
#include "bm_serial.h"
#include "bridgeLog.h"
#include "bridgePowerController.h"
#include "bristlemouth_client.h"
#include "bsp.h"
#include "cli.h"
#include "config_cbor_map_service.h"
#include "configuration.h"
#include "debug_bm_service.h"
#include "debug_bridge_power_controller.h"
#include "debug_configuration.h"
#include "debug_dfu.h"
#include "debug_gpio.h"
#include "debug_memfault.h"
#include "debug_ncp.h"
#include "debug_nvm_cli.h"
#include "debug_rtc.h"
#include "debug_spotter.h"
#include "debug_sys.h"
#include "debug_w25.h"
#include "device_info.h"
#include "external_flash_partitions.h"
#include "gpdma.h"
#include "gpioISR.h"
#include "memfault_platform_core.h"
#include "messages/neighbors.h"
#include "ncp_uart.h"
#include "nvmPartition.h"
#include "pca9535.h"
#include "pcap.h"
#include "printf.h"
#include "pubsub.h"
#include "ram_partitions.h"
#include "reportBuilder.h"
#include "sensorController.h"
#include "sensorSampler.h"
#include "sensors.h"
#include "serial.h"
#include "serial_console.h"
#include "stm32_rtc.h"
#include "sys_info_service.h"
#include "timer_callback_handler.h"
#include "topology_sampler.h"
#include "usb.h"
#include "w25.h"
#include "watchdog.h"
#ifdef RAW_PRESSURE_ENABLE
#include "rbrPressureProcessor.h"
#endif // RAW_PRESSURE_ENABLE
#ifdef USE_MICROPYTHON
#include "micropython_freertos.h"
#endif

// uncomment for the `update sec`, `update clr`, `update confirm` commands
//#include "mcuboot_cli.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifndef BSP_BRIDGE_V1_0
#error "BRIDGE BSP REQUIRED"
#endif

#define LED_BLUE LED_G
#define ALARM_OUT LED_R
#define USER_BUTTON BOOT

// LEDS are active low
#define LED_ON (0)
#define LED_OFF (1)

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

uint32_t sys_cfg_sensorsPollIntervalMs = DEFAULT_SENSORS_POLL_MS;
uint32_t sys_cfg_sensorsCheckIntervalS = DEFAULT_SENSORS_CHECK_S;
// TODO - make a getter API for these
NvmPartition *userConfigurationPartition = NULL;
NvmPartition *systemConfigurationPartition = NULL;
NvmPartition *hardwareConfigurationPartition = NULL;
NvmPartition *dfu_partition_global = NULL;
static IOPinHandle_t *usb_io = &LED_G;

static bool usb_is_connected() {
  uint8_t vusb = 0;

  IORead(usb_io, &vusb);

  return (bool)vusb;
}

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
                                DEFAULT_BOOT_TASK_PRIORITY, NULL);
  configASSERT(rval == pdTRUE);

  // Start FreeRTOS scheduler
  vTaskStartScheduler();

  /* We should never get here as control is now taken by the scheduler */

  while (1) {
  };
}

bool buttonPress(const void *pinHandle, uint8_t value, void *args) {
  (void)pinHandle;
  (void)args;

  if (value) {
    bm_pub(APP_PUB_SUB_BUTTON_TOPIC, APP_PUB_SUB_BUTTON_CMD_ON,
           sizeof(APP_PUB_SUB_BUTTON_CMD_ON) - 1, APP_PUB_SUB_BUTTON_TYPE,
           BM_COMMON_PUB_SUB_VERSION);
  } else {
    bm_pub(APP_PUB_SUB_BUTTON_TOPIC, APP_PUB_SUB_BUTTON_CMD_OFF,
           sizeof(APP_PUB_SUB_BUTTON_CMD_OFF) - 1, APP_PUB_SUB_BUTTON_TYPE,
           BM_COMMON_PUB_SUB_VERSION);
  }

  return false;
}

static void handle_subscriptions(uint64_t node_id, const char *topic, uint16_t topic_len,
                                 const uint8_t *data, uint16_t data_len, uint8_t type,
                                 uint8_t version) {
  (void)node_id;
  if (strncmp(APP_PUB_SUB_BUTTON_TOPIC, topic, topic_len) == 0) {
    if (type == APP_PUB_SUB_BUTTON_TYPE && version == APP_PUB_SUB_BUTTON_VERSION) {
      if (strncmp(APP_PUB_SUB_BUTTON_CMD_ON, reinterpret_cast<const char *>(data), data_len) ==
          0) {
        IOWrite(&LED_BLUE, LED_ON);
      } else if (strncmp(APP_PUB_SUB_BUTTON_CMD_OFF, reinterpret_cast<const char *>(data),
                         data_len) == 0) {
        IOWrite(&LED_BLUE, LED_OFF);
      } else {
        // Not handled
      }
    } else {
      printf("Unrecognized version: %u and type: %u\n", version, type);
    }
  } else if (strncmp(APP_PUB_SUB_PRINTF_TOPIC, topic, topic_len) == 0) {
    if (type == APP_PUB_SUB_PRINTF_TYPE && version == APP_PUB_SUB_PRINTF_VERSION) {
      printf("Topic: %.*s\n", topic_len, topic);
      printf("Data: %.*s\n", data_len, data);
      if (bm_serial_tx(BM_SERIAL_LOG, const_cast<uint8_t *>(data), data_len) != 0) {
        printf("Failed to forward to NCP.\n");
      }
    } else {
      printf("Unrecognized version: %u and type: %u\n", version, type);
    }
  } else if (strncmp(APP_PUB_SUB_UTC_TOPIC, topic, topic_len) == 0) {
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
        printf("Updating RTC to %u-%u-%u %02u:%02u:%02u.%04u\n", rtc_time.year, rtc_time.month,
               rtc_time.day, rtc_time.hour, rtc_time.minute, rtc_time.second, rtc_time.ms);
      } else {
        printf("\n Failed to set RTC.\n");
      }
    } else {
      printf("Unrecognized version: %u and type: %u\n", version, type);
    }
  } else if (strncmp(APP_PUB_SUB_LAST_NET_CFG_TOPIC, topic, topic_len) == 0) {
    if (type == APP_PUB_SUB_LAST_NET_CFG_TYPE && version == APP_PUB_SUB_LAST_NET_CFG_VERSION) {
      bm_topology_last_network_info_cb();
    } else {
      printf("Unrecognized version: %u and type: %u\n", version, type);
    }
  } else {
    printf("Topic: %.*s\n", topic_len, topic);
    printf("Data: %.*s\n", data_len, data);
  }
}

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

static void neighborDiscoveredCb(bool discovered, BcmpNeighbor *neighbor) {
  configASSERT(neighbor);
  const char *action = (discovered) ? "added" : "lost";
  bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_INFO, USE_HEADER,
                 "Neighbor %016" PRIx64 " %s\n", neighbor->node_id, action);
}

static void defaultTask(void *parameters) {
  (void)parameters;

  mxInit();

  usbMspInit();

  rtcInit();

  // Initialize low power manager
  lpmInit();

  // Inhibit low power mode during boot process
  lpmPeripheralActive(LPM_BOOT);

  startIWDGTask();
  startSerial();
  startSerialConsole(&usbCLI);
  // Serial device will be enabled automatically when console connects
  // so no explicit serialEnable is required
  pcapInit(&usbPcap);

  startCLI();

  gpioISRStartTask();

  memfault_platform_boot();
  memfault_platform_start();

  pca9535StartIRQTask();

  bspInit();

  debugSysInit();
  debugMemfaultInit(&usbCLI);
  // uncomment for the `update sec`, `update clr`, `update confirm` commands
  // mcubootCliInit();

  debugGpioInit(debugGpioPins, sizeof(debugGpioPins) / sizeof(DebugGpio_t));
  debugSpotterInit();
  debugRTCInit();

  timer_callback_handler_init();

  gpioISRRegisterCallback(&USER_BUTTON, buttonPress);

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
  debugDfuInit(&dfu_partition);
  bcl_init();

  uint32_t hw_version = 0;
  get_config_uint(BM_CFG_PARTITION_HARDWARE, AppConfig::HARDWARE_VERSION,
                  strlen(AppConfig::HARDWARE_VERSION), &hw_version);

  if (hw_version >= 7) {
    usb_io = &VUSB_DETECT;
  }

  setHwVersion(hw_version);

  usbInit(usb_io, usb_is_connected);
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

  printf("Using bridge power controller.\n");
  IOWrite(&BOOST_EN, 1);
  power_config_s pwrcfg = getPowerConfigs();
  BridgePowerController bridge_power_controller(
      VBUS_SW_EN, pwrcfg.sampleIntervalMs, pwrcfg.sampleDurationMs, pwrcfg.subsampleIntervalMs,
      pwrcfg.subsampleDurationMs, static_cast<bool>(pwrcfg.subsampleEnabled),
      static_cast<bool>(pwrcfg.bridgePowerControllerEnabled),
      (pwrcfg.alignmentInterval5Min * BridgePowerController::ALIGNMENT_INCREMENT_S),
      static_cast<bool>(pwrcfg.ticksSamplingEnabled));

  ncpInit(&usart3, &dfu_partition, &bridge_power_controller);
  topology_sampler_init(&bridge_power_controller);
  debug_ncp_init();
  debugBmServiceInit();
  sys_info_service_init();
  reportBuilderInit();
  sensorControllerInit(&bridge_power_controller);
  config_cbor_map_service_init();
  IOWrite(&ALARM_OUT, 1);
  IOWrite(&LED_BLUE, LED_OFF);

  bm_sub(APP_PUB_SUB_BUTTON_TOPIC, handle_subscriptions);
  bm_sub(APP_PUB_SUB_PRINTF_TOPIC, handle_subscriptions);
  bm_sub(APP_PUB_SUB_UTC_TOPIC, handle_subscriptions);
  bm_sub(APP_PUB_SUB_LAST_NET_CFG_TOPIC, handle_subscriptions);
  bcmp_neighbor_register_discovery_callback(neighborDiscoveredCb);

#ifdef USE_MICROPYTHON
  micropython_freertos_init(&usbCLI);
#endif

#ifdef RAW_PRESSURE_ENABLE
  raw_pressure_config_s raw_pressure_cfg = getRawPressureConfigs();
  rbrPressureProcessorInit(raw_pressure_cfg.rawSampleS, raw_pressure_cfg.maxRawReports,
                           raw_pressure_cfg.rawDepthThresholdUbar,
                           raw_pressure_cfg.rbrCodaReadingPeriodMs);
#endif // RAW_PRESSURE_ENABLE

  // // Re-enable low power mode
  lpmPeripheralInactive(LPM_BOOT);

  // Drop priority now that we're done booting
  vTaskPrioritySet(xTaskGetCurrentTaskHandle(), DEFAULT_TASK_PRIORITY);

  while (1) {
    /* Do nothing */
    vTaskDelay(1000);
  }
}
