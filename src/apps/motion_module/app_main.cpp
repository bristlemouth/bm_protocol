#include "main.h"

#include "bm_os.h"
#include "spotter.h"
#include "task_priorities.h"

#include "app_pub_sub.h"
#include "app_util.h"
#include "bristlemouth_client.h"
#include "bsp.h"
#include "cli.h"
extern "C" {
#include "config_cbor_map_service.h"
#include "echo_service.h"
#include "sys_info_service.h"
}
#include "bm_config.h"
#include "debug_bm_service.h"
#include "debug_configuration.h"
#include "debug_dfu.h"
#include "debug_memfault.h"
#include "debug_nvm_cli.h"
#include "debug_rtc.h"
#include "debug_spotter.h"
#include "debug_sys.h"
#include "debug_w25.h"
#include "external_flash_partitions.h"
#include "gpioISR.h"
#include "memfault_platform_core.h"
#include "nvmPartition.h"
#include "pca9535.h"
#include "pcap.h"
#include "pubsub.h"
#include "sensorSampler.h"
#include "sensors.h"
#include "serial.h"
#include "serial_console.h"
#include "stm32_rtc.h"
#include "timer_callback_handler.h"
#include "usb.h"
#include "w25.h"
#include <inttypes.h>

//#include "motionSampler.h"

#define LED_ON (0)
#define LED_OFF (1)
#define bmdk_log_filename "bmdk.log"

#include <stdio.h>
#include <string.h>

/* re-enable some warnings that bm_protocol (inadvertently?) disables as of this writing */
#pragma GCC diagnostic warning "-Wall"
#pragma GCC diagnostic warning "-Wextra"
#pragma GCC diagnostic warning "-Wshadow"
#pragma GCC diagnostic warning "-Wformat"

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
    .arg = NULL,
    .enabled = false,
    .flags = 0,
    .preTxCb = NULL,
    .postTxCb = NULL,
    .breakISR = NULL,
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
    .arg = NULL,
    .enabled = false,
    .flags = 0,
    .preTxCb = NULL,
    .postTxCb = NULL,
    .breakISR = NULL,
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
    .arg = NULL,
    .enabled = false,
    .flags = 0,
    .preTxCb = NULL,
    .postTxCb = NULL,
    .breakISR = NULL,
};

// TODO - make a getter API for these
NvmPartition *userConfigurationPartition = NULL;
NvmPartition *systemConfigurationPartition = NULL;
NvmPartition *hardwareConfigurationPartition = NULL;
NvmPartition *dfu_partition_global = NULL;

uint32_t sys_cfg_sensorsPollIntervalMs = DEFAULT_SENSORS_POLL_MS;
uint32_t sys_cfg_sensorsCheckIntervalS = DEFAULT_SENSORS_CHECK_S;

extern "C" void USART3_IRQHandler(void) { serialGenericUartIRQHandler(&usart3); }

// Only needed if we want the debug commands too
// extern MS5803 debugPressure;
// extern HTU21D debugHTU;
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

  BmErr err = bm_task_create(defaultTask, "Default", 4096, NULL, 2, NULL);
  configASSERT(err == BmOK);
  bm_start_scheduler();

  while (1) {
  };
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

static void defaultTask(void *parameters) {
  (void)parameters;

  mxInit();

  usbMspInit();

  rtcInit();

  // Initialize low power manager
  lpmInit();

  // Inhibit low power mode during boot process
  lpmPeripheralActive(LPM_BOOT);

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
  usbInit(&VUSB_DETECT, usb_is_connected);

  debugSysInit();
  debugMemfaultInit(&usbCLI);

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
  debugDfuInit(&dfu_partition);
  bcl_init();
  get_config_uint(BM_CFG_PARTITION_SYSTEM, "sensorsPollIntervalMs",
                  strlen("sensorsPollIntervalMs"), &sys_cfg_sensorsPollIntervalMs);
  get_config_uint(BM_CFG_PARTITION_SYSTEM, "sensorsCheckIntervalS",
                  strlen("sensorsCheckIntervalS"), &sys_cfg_sensorsCheckIntervalS);
  debugConfigurationInit();

  sensorConfig_t sensorConfig = {.sensorCheckIntervalS = sys_cfg_sensorsCheckIntervalS,
                                 .sensorsPollIntervalMs = sys_cfg_sensorsPollIntervalMs};
  sensorSamplerInit(&sensorConfig);
  // must call sensorsInit after sensorSamplerInit
  sensorsInit();
  debugBmServiceInit();

  bm_sub(APP_PUB_SUB_UTC_TOPIC, handle_bm_subscriptions);
  echo_service_init();
  sys_info_service_init();
  config_cbor_map_service_init();

  // Re-enable low power mode
  lpmPeripheralInactive(LPM_BOOT);

  while (1) {
    sensorsHandle();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
