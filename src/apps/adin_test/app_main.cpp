
// Includes from CubeMX Generated files
#include "main.h"

// Peripheral
#include "adc.h"
#include "gpio.h"
#include "icache.h"
#include "iwdg.h"
#include "rtc.h"
#include "usart.h"
#include "usb_otg.h"

// Includes for FreeRTOS
#include "FreeRTOS.h"
#include "task.h"

#include "bm_l2.h"
#include "bristlemouth.h"
#include "bsp.h"
#include "cli.h"
#include "debug_bm.h"
#include "debug_memfault.h"
#include "debug_sys.h"
#include "gpioISR.h"
#include "memfault_platform_core.h"
#include "pcap.h"
#include "printf.h"
#include "serial.h"
#include "serial_console.h"
#include "usb.h"
#include "watchdog.h"
#include "bm_pubsub.h"


#include <stdio.h>
#include <string.h>

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

const char* publication_topics = "button";

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
    MX_USART1_UART_Init();
    MX_USB_OTG_FS_PCD_Init();
    MX_ICACHE_Init();
    MX_RTC_Init();
    MX_IWDG_Init();

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

const char topic[] = "button";
const char on_str[] = "on";
const char off_str[] = "off";

bool buttonPress(const void *pinHandle, uint8_t value, void *args) {
    (void)pinHandle;
    (void)args;

    bm_pub_t publication;


    publication.topic = (char *) topic;
    publication.topic_len = sizeof(topic) - 1; // Don't care about Null terminator

    if(value) {
        publication.data = (char *)on_str;
        publication.data_len = sizeof(on_str) - 1; // Don't care about Null terminator
    } else {
        publication.data = (char *)off_str;
        publication.data_len = sizeof(off_str) - 1; // Don't care about Null terminator
    }

    bm_pubsub_publish(&publication);

    return false;
}

void handle_sensor_subscriptions(char* topic, uint16_t topic_len, char* data, uint16_t data_len) {
    if (strncmp("button", topic, topic_len) == 0) {
        if (strncmp("on", data, data_len) == 0) {
            IOWrite(&LED_BLUE, 1);
            IOWrite(&ALARM_OUT, 1);
        } else if (strncmp("off", data, data_len) == 0) {
            IOWrite(&LED_BLUE, 0);
            IOWrite(&ALARM_OUT, 0);
        } else {
            // Not handled
        }
    } else {
        printf("Topic: %.*s\n", topic_len, topic);
        printf("Data: %.*s\n", data_len, data);
    }
}

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
      dfuSerial = &usbPcap;
#else
      pcapInit(&usbPcap);
#endif

    } else {
      startSerialConsole(&usart1);

#if BM_DFU_HOST
      printf("WARNING: USB must be connected to use DFU mode. This serial port will be used by the serial console instead.\n");
#endif

      printf("WARNING: PCAP support requires USB connection.\n");
    }
    startCLI();
    // pcapInit(&usbPcap);
    serialEnable(&usart1);
    gpioISRStartTask();

    memfault_platform_boot();
    memfault_platform_start();

    bspInit();

    usbInit();

    debugSysInit();
    debugMemfaultInit(&usart1);
    debugBMInit();

    // Commenting out while we test usart1
    // lpmPeripheralInactive(LPM_BOOT);

    gpioISRRegisterCallback(&USER_BUTTON, buttonPress);

    bcl_init(dfuSerial);

    IOWrite(&ALARM_OUT, 0);

    bm_sub_t subscription;
    const char topic[] = "button";

    subscription.topic = (char *) topic;
    subscription.topic_len = sizeof(topic) - 1; // Don't care about Null terminator
    subscription.cb = handle_sensor_subscriptions;
    bm_pubsub_subscribe(&subscription);

    while(1) {
        /* Do nothing */
        vTaskDelay(1000);
    }
}
