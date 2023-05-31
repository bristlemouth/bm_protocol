
// Includes from CubeMX Generated files
#include "main.h"

// Peripheral
#include "adc.h"
#include "gpdma.h"
#include "gpio.h"
#include "icache.h"
#include "iwdg.h"
// #include "ucpd.h"
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
#include "debug_rtc.h"
#include "debug_sys.h"
#include "gpioISR.h"
#include "memfault_platform_core.h"
#include "mic.h"
#include "pcap.h"
#include "pca9535.h"
#include "printf.h"
#include "serial.h"
#include "serial_console.h"
#include "stm32_rtc.h"
#include "task_priorities.h"
#include "usb.h"
#include "util.h"
#include "watchdog.h"


#include <stdio.h>

#ifdef BSP_DEV_MOTE_V1_0
    #define LED_BLUE EXP_LED_G1
    #define ALARM_OUT GPIO1
    #define USER_BUTTON GPIO2
    #define LED_ON (0)
    #define LED_OFF (1)
#elif BSP_DEV_MOTE_HYDROPHONE
    #define LED_BLUE EXP_LED_G1
    #define ALARM_OUT GPIO1
    #define USER_BUTTON GPIO2
    #define LED_ON (0)
    #define LED_OFF (1)
#else
    #define LED_ON (1)
    #define LED_OFF (0)
#endif // BSP_DEV_MOTE_V1_0

static void defaultTask(void *parameters);

// Serial console (when no usb present)
#ifndef NO_UART
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
#endif

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
  .txBufferSize = 4096,
  .rxBufferSize = 64,
  .rxBytesFromISR = NULL,
  .getTxBytesFromISR = NULL,
  .processByte = NULL,
  .data = NULL,
  .enabled = false,
  .flags = 0,
};

const char* publication_topics = "";

#ifndef NO_UART
extern "C" void USART1_IRQHandler(void) {
  serialGenericUartIRQHandler(&usart1);
}
#endif // NO_UART

extern "C" int main(void) {

  // Before doing anything, check if we should enter ROM bootloader
  // enterBootloaderIfNeeded();

  HAL_Init();

  SystemClock_Config();

  SystemPower_Config_ext();
  MX_GPIO_Init();
  // MX_UCPD1_Init();
#ifndef NO_UART
  MX_USART1_UART_Init();
#endif
  MX_USB_OTG_FS_PCD_Init();
  MX_ICACHE_Init();
  MX_GPDMA1_Init();
  MX_IWDG_Init();

  usbMspInit();

  rtcInit();

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

void buttonTopicSubscription(uint64_t node_id, const char* topic, uint16_t topic_len, const uint8_t* data, uint16_t data_len) {
  (void)node_id;
    if (strncmp("button", topic, topic_len) == 0) {
        if (strncmp("on", reinterpret_cast<const char *>(data), data_len) == 0) {
#if BSP_DEV_MOTE_HYDROPHONE
            IOWrite(&EXP_LED_R1, LED_ON);
#else
            IOWrite(&LED_BLUE, LED_ON);
#endif
        } else if (strncmp("off", reinterpret_cast<const char *>(data), data_len) == 0) {
#if BSP_DEV_MOTE_HYDROPHONE
            IOWrite(&EXP_LED_R1, LED_OFF);
#else
            IOWrite(&LED_BLUE, LED_OFF);
#endif
        } else {
            // Not handled
        }
    }
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

  startIWDGTask();

  // Using usbPcap for streaming audio
  // pcapInit(&usbPcap);

  gpioISRStartTask();

  memfault_platform_boot();
  memfault_platform_start();

  pca9535StartIRQTask();

  bspInit();

  startSerial();
  // Use USB for serial console if USB is connected on boot
  // Otherwise use ST-Link serial port
  if(usb_is_connected()) {
    startSerialConsole(&usbCLI);
    // Serial device will be enabled automatically when console connects
    // so no explicit serialEnable is required

  } else {
#ifndef NO_UART
    startSerialConsole(&usart1);
    serialEnable(&usart1);
#endif
  }
  startCLI();

#ifdef BSP_NUCLEO_U575
    // We don't have a vusb_detect interrupt line on the nucleo board
    usbInit(NULL, usb_is_connected);

    // USB detect is an analog signal on the NUCLEO, so we can't use
    // an interrupt to detect when usb is connected/disconnected
    // So low power mode will not be enabled
    lpmPeripheralActive(LPM_USB);
#else
    usbInit(&VUSB_DETECT, usb_is_connected);
#endif

  debugSysInit();
  debugMemfaultInit(&usbCLI);
  debugBMInit();
  debugRTCInit();

#ifndef BSP_NUCLEO_U575
  IOWrite(&EXP_LED_G1, LED_OFF);
  IOWrite(&EXP_LED_R1, LED_OFF);
  IOWrite(&EXP_LED_G2, LED_OFF);
  IOWrite(&EXP_LED_R2, LED_OFF);
#endif

  // Re-enable low power mode
  lpmPeripheralInactive(LPM_BOOT);

  gpioISRRegisterCallback(&USER_BUTTON, buttonPress);

  bcl_init(NULL, NULL, NULL);

  // Drop priority now that we're done booting
  vTaskPrioritySet(xTaskGetCurrentTaskHandle(), DEFAULT_TASK_PRIORITY);

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

const char buttonTopic[] = "button";
const char hydroDbTopic[] = "hydrophone/db";
const char hydroStreamTopic[] = "hydrophone/stream";
const char hydroStreamEnableTopic[] = "hydrophone/stream/enable";
const char alarmDurationTopic[] = "alarm/duration";
const char alarmThresholdTopic[] = "alarm/threshold";
const char alarmTriggerTopic[] = "alarm/trigger";

volatile static uint32_t alarmDBThreshold;
volatile static uint32_t alarmDurationS;

bool streamEnabled = false;

#define AUDIO_MAGIC 0xAD10B055

// buffer for resized mic samples (int16_t)
#define MIC_SAMPLES_PER_PACKET (512)
typedef struct {
  uint32_t magic;
  uint32_t sampleRate;
  uint16_t numSamples;
  uint8_t sampleSize;
  uint8_t flags;
  int16_t samples[0];
} __attribute__((__packed__)) hydrophoneStreamDataHeader_t;

typedef struct {
  hydrophoneStreamDataHeader_t header;
  int16_t samples[MIC_SAMPLES_PER_PACKET];
} __attribute__((__packed__)) hydrophoneStreamData_t;

static hydrophoneStreamData_t streamData;

static bool processMicSamples(const uint32_t *samples, uint32_t numSamples, void *args) {
  (void)args;
  (void)samples;
  (void)numSamples;
  float dbLevel = micGetDB(samples, numSamples);

  bm_pub(hydroDbTopic, &dbLevel, sizeof(float));

  if(streamEnabled) {
    for(uint32_t idx = 0; idx < MIN(numSamples, MIC_SAMPLES_PER_PACKET); idx++) {
      streamData.samples[idx] = (int16_t)(samples[idx] >> 8);
    }
    streamData.header.numSamples = MIN(numSamples, MIC_SAMPLES_PER_PACKET);

    bm_pub(hydroStreamTopic,
              &streamData.header,
              (sizeof(hydrophoneStreamDataHeader_t) + sizeof(int16_t) * streamData.header.numSamples));

    if(numSamples > MIC_SAMPLES_PER_PACKET) {
      for(uint32_t idx = MIC_SAMPLES_PER_PACKET; idx < numSamples; idx++) {
        streamData.samples[idx - MIC_SAMPLES_PER_PACKET] = (int16_t)(samples[idx] >> 8);
      }
      streamData.header.numSamples = (numSamples - MIC_SAMPLES_PER_PACKET);
      bm_pub(hydroStreamTopic,
              &streamData.header,
              (sizeof(hydrophoneStreamDataHeader_t) + sizeof(int16_t) * streamData.header.numSamples));
    }
  }

  return true;
}

void printDbData(uint64_t node_id, const char* topic, uint16_t topic_len, const uint8_t* data, uint16_t data_len) {
  (void)node_id;
  (void)topic;
  (void)topic_len;
  if(data_len == sizeof(float)) {
    float dbLevel = 0;
    memcpy(&dbLevel, data, sizeof(dbLevel));
    if(dbLevel >= alarmDBThreshold) {
      alarmTimer = alarmDurationS * 100;
    }
  }
}

void streamAudioData(uint64_t node_id, const char* topic, uint16_t topic_len, const uint8_t* data, uint16_t data_len) {
  (void)node_id;
  (void)topic;
  (void)topic_len;

  if(usbPcap.enabled) {
    serialWrite(&usbPcap, const_cast<uint8_t *>(data), data_len);
  }
}

void streamEnable(uint64_t node_id, const char* topic, uint16_t topic_len, const uint8_t* data, uint16_t data_len) {
  (void)node_id;
  (void)topic;
  (void)topic_len;
  (void)data_len;

  if((data[0] == '1') || (memcmp("on", data, 2) == 0)) {
    streamEnabled = true;
  } else {
    streamEnabled = false;
  }
}

// Manually trigger alarm
void alarmTriggerCb(uint64_t node_id, const char* topic, uint16_t topic_len, const uint8_t* data, uint16_t data_len) {
  (void)node_id;
  (void)topic;
  (void)topic_len;
  (void)data;
  (void)data_len;
  alarmTimer = alarmDurationS * 100; // we use a 10ms loop, so *100 for the timer proper
}

// Update alarm threshold from data
void updateThresholdCb(uint64_t node_id, const char* topic, uint16_t topic_len, const uint8_t* data, uint16_t data_len) {
  (void)node_id;
  (void)topic;
  (void)topic_len;

  if(data && data_len > 0) {
    uint32_t newThreshold = strtoul(reinterpret_cast<const char *>(data), 0, 10);
    if(newThreshold > 0 && newThreshold <= 200) {
      printf("Updating alarm threshold to %" PRIu32 "dB\n", newThreshold);
      alarmDBThreshold = newThreshold;
    }
  }
}

// Update alarm duration from data
void updateDurationCb(uint64_t node_id, const char* topic, uint16_t topic_len, const uint8_t* data, uint16_t data_len) {
  (void)node_id;
  (void)topic;
  (void)topic_len;

  if(data && data_len > 0) {
    uint32_t newDuration = strtoul(reinterpret_cast<const char *>(data), 0, 10);

    // Max duration of 2 minutes
    if(newDuration <= (60 * 2)) {
      printf("Updating alarm duration to %" PRIu32 "dB\n", newDuration);
      alarmDurationS = newDuration;
    }
  }
}

extern SAI_HandleTypeDef hsai_BlockA1;

static bool prevAlarmState = false;
// Only write alarm state if state has changed
// Since IO's go through IO expander, this saves on a LOT
// of IO expander comms
void setAlarmState(bool state) {
  if(state != prevAlarmState){
    prevAlarmState = state;
    if(state) {
      IOWrite(&ALARM_OUT, 1);
      IOWrite(&LED_BLUE, LED_ON);
    } else {
      IOWrite(&ALARM_OUT, 0);
      IOWrite(&LED_BLUE, LED_OFF);
    }
  }
}

static void hydrophoneTask( void *parameters ) {
  (void)parameters;
  vTaskDelay(1000);

  // memset(str1, ' ', sizeof(str1));

  // Default dB threshold (aka don't turn on at all)
  alarmDBThreshold = 200;

  // Default alarm duration (1 second)
  alarmDurationS = 1;

  // These won't change
  streamData.header.magic = AUDIO_MAGIC;
  streamData.header.sampleRate = 50000;
  streamData.header.sampleSize = 2;

  if(micInit(&hsai_BlockA1, NULL)) {
    publication_topics = "hydrophone/db | hydrophone/stream";

    // Hydrophone audio stream enable/disable

    bm_sub(hydroStreamEnableTopic, streamEnable);
    bm_sub(buttonTopic, buttonTopicSubscription);

    while(1) {
      // "sample long time"
      micSample(50000, processMicSamples, NULL);
    }
  } else {
    printf("Microphone not detected. Defaulting to listener.\n");

    // Single byte trigger level for fast response time
    usbPcap.txStreamBuffer = xStreamBufferCreate(usbPcap.txBufferSize, 1);
    configASSERT(usbPcap.txStreamBuffer != NULL);

    usbPcap.rxStreamBuffer = xStreamBufferCreate(usbPcap.rxBufferSize, 1);
    configASSERT(usbPcap.rxStreamBuffer != NULL);

    // serialEnable(&usbPcap);

    // Hydrophone dB level
    bm_sub(hydroDbTopic, printDbData);

    // alarm threshold level
    bm_sub(alarmThresholdTopic, updateThresholdCb);

    // alarm duration
    bm_sub(alarmDurationTopic, updateDurationCb);

    // alarm duration in seconds
    bm_sub(alarmTriggerTopic, alarmTriggerCb);

    bm_sub(buttonTopic, buttonTopicSubscription);

    while(1) {
      if(alarmTimer > 0) {
        alarmTimer--;
        setAlarmState(true);
      } else {
        setAlarmState(false);

      }
      vTaskDelay(10);
    }
  }
}

void usb_line_state_change(uint8_t itf, uint8_t dtr, bool rts) {
  (void)rts;

  SerialHandle_t *handle = serialHandleFromItf(itf);
  configASSERT(handle);
  switch(itf) {
    case 0: {
      if ( dtr ) {
        // Terminal connected
        serialConsoleEnable();
      } else {
        // Terminal disconnected
        serialConsoleDisable();
      }
      break;
    }

    case 1: {
      if ( dtr ) {
        // Enable audio streaming

        // Hydrophone audio stream!
        bm_sub(hydroStreamTopic, streamAudioData);

        serialEnable(&usbPcap);
        xStreamBufferReset(usbPcap.txStreamBuffer);
        xStreamBufferReset(usbPcap.rxStreamBuffer);

        bm_pub(hydroStreamEnableTopic, "1", 1);
        printf("bm pub %s 1\n", hydroStreamEnableTopic);
      } else {
        // Disable audio streaming
        bm_unsub(hydroStreamTopic, streamAudioData);
        serialDisable(&usbPcap);

        bm_pub(hydroStreamEnableTopic, "0", 1);
        printf("bm pub %s 0\n", hydroStreamEnableTopic);
      }
      break;
    }

    default:
      configASSERT(0);
  }
}
