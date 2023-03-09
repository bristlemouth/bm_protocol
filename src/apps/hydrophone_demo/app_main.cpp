
// Includes from CubeMX Generated files
#include "main.h"

// Peripheral
#include "adc.h"
#include "gpdma.h"
#include "gpio.h"
#include "icache.h"
#include "iwdg.h"
#include "rtc.h"
#include "ucpd.h"
#include "usart.h"
#include "usb_otg.h"

// Includes for FreeRTOS
#include "FreeRTOS.h"
#include "task.h"

#include "bm_l2.h"
#include "bm_pubsub.h"
#include "bristlemouth.h"
#include "bsp.h"
#include "cli.h"
#include "debug_bm.h"
#include "debug_memfault.h"
#include "debug_middleware.h"
#include "debug_sys.h"
#include "gpioISR.h"
#include "memfault_platform_core.h"
#include "mic.h"
#include "pcap.h"
#include "printf.h"
#include "serial.h"
#include "serial_console.h"
#include "task_priorities.h"
#include "usb.h"
#include "watchdog.h"


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
  .txBufferSize = 1024,
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

  BaseType_t rval = xTaskCreate(defaultTask,
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
  return false;
}

static void hydrophoneTask( void *parameters );

volatile uint32_t alarmTimer;

static void defaultTask( void *parameters ) {
  (void)parameters;
  SerialHandle_t *dfuSerial = NULL;

  startIWDGTask();
  startSerial();
  // Use USB for serial console if USB is connected on boot
  // Otherwise use ST-Link serial port
  if(usb_is_connected()) {
    startSerialConsole(&usbCLI);
    // Serial device will be enabled automatically when console connects
    // so no explicit serialEnable is required

#if BM_DFU_HOST
    dfuSerial = &usart1;
    serialEnable(&usart1);
#endif

  } else {
    startSerialConsole(&usart1);

#if BM_DFU_HOST
    printf("WARNING: USB must be connected to use DFU mode. This serial port will be used by the serial console instead.\n");
#endif

    printf("WARNING: PCAP support requires USB connection.\n");
  }
  startCLI();
  pcapInit(&usbPcap);
  serialEnable(&usart1);
  gpioISRStartTask();

  memfault_platform_boot();
  memfault_platform_start();

  bspInit();

  usbInit();

  debugSysInit();
  debugMemfaultInit(&usart1);
  debugBMInit();
  debugMiddlewareInit();

  // Commenting out while we test usart1
  // lpmPeripheralInactive(LPM_BOOT);

  gpioISRRegisterCallback(&USER_BUTTON, buttonPress);

  bcl_init(dfuSerial);

  BaseType_t rval = xTaskCreate(hydrophoneTask,
                  "hydrophone",
                  // TODO - verify stack size
                  128 * 4,
                  NULL,
                  MIC_TASK_PRIORITY,
                  NULL);
  configASSERT(rval == pdTRUE);

  while(1) {
    vTaskDelay(1000000);
  }
}

bm_pub_t hydroDbPub;
const char hydroDbTopic[] = "hydrophone/db";
const char alarmDurationTopic[] = "alarm/duration";
const char alarmThresholdTopic[] = "alarm/threshold";
const char alarmTriggerTopic[] = "alarm/trigger";

volatile static uint32_t alarmDBThreshold;
volatile static uint32_t alarmDurationS;

static bool processMicSamples(const uint32_t *samples, uint32_t numSamples, void *args) {
  (void)args;
  float dbLevel = micGetDB(samples, numSamples);
  hydroDbPub.data = (char *)&dbLevel;
  hydroDbPub.data_len = sizeof(float);
  bm_pubsub_publish(&hydroDbPub);

  return true;
}

void printDbData(char* topic, uint16_t topic_len, char* data, uint16_t data_len) {
  (void)topic;
  (void)topic_len;
  if(data_len == sizeof(float)) {
    float *dbLevel = (float *)data;
    printf("RX %0.1f dB\n", *dbLevel);
    if(*dbLevel >= alarmDBThreshold) {
      alarmTimer = alarmDurationS * 100;
    }
  }
}

// Manually trigger alarm
void alarmTriggerCb(char* topic, uint16_t topic_len, char* data, uint16_t data_len) {
  (void)topic;
  (void)topic_len;
  (void)data;
  (void)data_len;
  alarmTimer = alarmDurationS * 100; // we use a 10ms loop, so *100 for the timer proper
}

// Update alarm threshold from data
void updateThresholdCb(char* topic, uint16_t topic_len, char* data, uint16_t data_len) {
  (void)topic;
  (void)topic_len;

  if(data && data_len > 0) {
    uint32_t newThreshold = strtoul(data, 0, 10);
    if(newThreshold > 0 && newThreshold <= 200) {
      printf("Updating alarm threshold to %ludB\n", newThreshold);
      alarmDBThreshold = newThreshold;
    }
  }
}

// Update alarm duration from data
void updateDurationCb(char* topic, uint16_t topic_len, char* data, uint16_t data_len) {
  (void)topic;
  (void)topic_len;

  if(data && data_len > 0) {
    uint32_t newDuration = strtoul(data, 0, 10);

    // Max duration of 2 minutes
    if(newDuration <= (60 * 2)) {
      printf("Updating alarm duration to %ludB\n", newDuration);
      alarmDurationS = newDuration;
    }
  }
}

extern SAI_HandleTypeDef hsai_BlockA1;

static void hydrophoneTask( void *parameters ) {
  (void)parameters;
  vTaskDelay(1000);

  // Default dB threshold (aka don't turn on at all)
  alarmDBThreshold = 200;

  // Default alarm duration (1 second)
  alarmDurationS = 1;

  hydroDbPub.topic = (char *)hydroDbTopic;
  hydroDbPub.topic_len = sizeof(hydroDbTopic) - 1;

  if(micInit(&hsai_BlockA1, NULL)) {
    while(1) {
      // "sample long time"
      micSample(50000, processMicSamples, NULL);
    }
  } else {
    printf("Microphone not detected. Defaulting to listener.\n");

    bm_sub_t sub;
    // Hydrophone dB level
    sub.topic = (char *)hydroDbTopic;
    sub.topic_len = sizeof(hydroDbTopic) - 1;
    sub.cb = printDbData;
    bm_pubsub_subscribe(&sub);

    // alarm threshold level
    sub.topic = (char *)alarmThresholdTopic;
    sub.topic_len = sizeof(alarmThresholdTopic) - 1;
    sub.cb = updateThresholdCb;
    bm_pubsub_subscribe(&sub);

    // alarm duration
    sub.topic = (char *)alarmDurationTopic;
    sub.topic_len = sizeof(alarmDurationTopic) - 1;
    sub.cb = updateDurationCb;
    bm_pubsub_subscribe(&sub);

    // alarm duration in seconds
    sub.topic = (char *)alarmTriggerTopic;
    sub.topic_len = sizeof(alarmTriggerTopic) - 1;
    sub.cb = alarmTriggerCb;
    bm_pubsub_subscribe(&sub);

    while(1) {
      if(alarmTimer > 0) {
        alarmTimer--;
        IOWrite(&ALARM_OUT, 1);
        IOWrite(&LED_BLUE, 1);
      } else {
        IOWrite(&ALARM_OUT, 0);
        IOWrite(&LED_BLUE, 0);
      }
      vTaskDelay(10);
    }
  }
}
