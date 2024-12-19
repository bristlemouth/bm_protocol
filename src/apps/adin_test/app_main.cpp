
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

#include "app_pub_sub.h"
#include "app_util.h"
#include "bristlemouth_client.h"
#include "bsp.h"
#include "cli.h"
#include "debug_configuration.h"
#include "debug_dfu.h"
#include "debug_gpio.h"
#include "debug_memfault.h"
#include "debug_nvm_cli.h"
#include "debug_rtc.h"
#include "debug_spotter.h"
#include "debug_sys.h"
#include "debug_w25.h"
#include "external_flash_partitions.h"
#include "gpdma.h"
#include "gpioISR.h"
#include "l2.h"
#include "memfault_platform_core.h"
#include "nvmPartition.h"
#include "pca9535.h"
#include "pcap.h"
#include "printf.h"
#include "pubsub.h"
#include "ram_partitions.h"
#include "serial.h"
#include "serial_console.h"
#include "stm32_rtc.h"
#include "stress.h"
#include "timer_callback_handler.h"
#include "usb.h"
#include "w25.h"
#include "watchdog.h"

#include <stdio.h>
#include <string.h>

#ifdef BSP_DEV_MOTE_V1_0
#define LED_BLUE EXP_LED_G1
#define ALARM_OUT EXP_LED_R2
#define USER_BUTTON GPIO2

// LEDS are active low
#define LED_ON (0)
#define LED_OFF (1)
#elif BSP_BRIDGE_V1_0
#define LED_BLUE LED_G
#define ALARM_OUT LED_R
#define USER_BUTTON BOOT

// LEDS are active low
#define LED_ON (0)
#define LED_OFF (1)
#elif BSP_MOTE_V1_0
#define LED_BLUE GPIO1
#define ALARM_OUT GPIO2
#define USER_BUTTON BOOT_LED

// LEDS are active low
#define LED_ON (0)
#define LED_OFF (1)
#else
#define LED_ON (1)
#define LED_OFF (0)
#endif // BSP_DEV_MOTE_V1_0

static void defaultTask(void *parameters);

// Serial console (when no usb present)
SerialHandle_t usart1 = {
    .device = USART1,
    .name = "usart1",
    .txPin = NULL,
    .rxPin = NULL,
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

#ifndef BSP_MOTE_V1_0
extern "C" void USART1_IRQHandler(void) { serialGenericUartIRQHandler(&usart1); }
#endif // BM_MOTE_V1_0

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

  BaseType_t rval = xTaskCreate(
      defaultTask, "Default",
      128 * 4, // TODO - verify stack size
      NULL,
      2, // Start with very high priority during boot then downgrade once done initializing everything
      NULL);
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

bool buttonPress(const void *pinHandle, uint8_t value, void *args) {
  (void)pinHandle;
  (void)args;

  if (value) {
    bm_pub(APP_PUB_SUB_BUTTON_TOPIC, APP_PUB_SUB_BUTTON_CMD_ON,
           sizeof(APP_PUB_SUB_BUTTON_CMD_ON) - 1, APP_PUB_SUB_BUTTON_TYPE,
           APP_PUB_SUB_BUTTON_VERSION);
  } else {
    bm_pub(APP_PUB_SUB_BUTTON_TOPIC, APP_PUB_SUB_BUTTON_CMD_OFF,
           sizeof(APP_PUB_SUB_BUTTON_CMD_OFF) - 1, APP_PUB_SUB_BUTTON_TYPE,
           APP_PUB_SUB_BUTTON_VERSION);
  }

  return false;
}

void handle_subscriptions(uint64_t node_id, const char *topic, uint16_t topic_len,
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
  } else {
    printf("Topic: %.*s\n", topic_len, topic);
    printf("Data: %.*s\n", data_len, data);
  }
}

#ifdef BSP_BRIDGE_V1_0
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
#endif

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

  usbInit(&VUSB_DETECT, usb_is_connected);

  debugSysInit();
  debugMemfaultInit(&usbCLI);

#ifdef BSP_BRIDGE_V1_0
  debugGpioInit(debugGpioPins, sizeof(debugGpioPins) / sizeof(DebugGpio_t));
#endif
  debugSpotterInit();
  debugRTCInit();
  timer_callback_handler_init();

  gpioISRRegisterCallback(&USER_BUTTON, buttonPress);
#ifdef BSP_DEV_MOTE_V1_0
  gpioISRRegisterCallback(&BOOT_LED, start_stress);
#endif
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
  debugConfigurationInit();

#ifdef BSP_BRIDGE_V1_0
  printf("Enabling 24V!\n");
  IOWrite(&BOOST_EN, 1);
  IOWrite(&VBUS_SW_EN, 1);
#endif

  IOWrite(&ALARM_OUT, 1);
  IOWrite(&LED_BLUE, LED_OFF);
#ifdef BSP_DEV_MOTE_V1_0
  IOWrite(&EXP_LED_G2, LED_OFF);
  IOWrite(&EXP_LED_R1, LED_OFF);
#endif // BSP_DEV_MOTE_V1_0
#ifdef BSP_MOTE_V1_0
  IOWrite(&BF_LED_G2, LED_OFF);
  IOWrite(&BF_LED_R2, LED_OFF);
  IOWrite(&BF_LED_G1, LED_OFF);
  IOWrite(&BF_LED_R1, LED_OFF);
#endif // BSP_DEV_MOTE_V1_0
  vTaskDelay(1000);
#ifdef BSP_MOTE_V1_0
  IOWrite(&BF_LED_G2, LED_ON);
  IOWrite(&BF_LED_R2, LED_OFF);
  IOWrite(&BF_LED_G1, LED_ON);
  IOWrite(&BF_LED_R1, LED_OFF);
#endif // BSP_DEV_MOTE_V1_0

  bm_sub(APP_PUB_SUB_BUTTON_TOPIC, handle_subscriptions);
  bm_sub(APP_PUB_SUB_UTC_TOPIC, handle_subscriptions);

  // Re-enable low power mode
  // lpmPeripheralInactive(LPM_BOOT);

  while (1) {
    /* Do nothing */
    vTaskDelay(1000);
  }
}
