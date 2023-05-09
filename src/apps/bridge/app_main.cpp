
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

#include "bm_l2.h"
#include "bm_pubsub.h"
#include "bristlemouth.h"
#include "bsp.h"
#include "cli.h"
#include "debug_bm.h"
#include "debug_dfu.h"
#include "debug_gpio.h"
#include "debug_memfault.h"
#include "debug_ncp.h"
#include "debug_nvm_cli.h"
#include "debug_rtc.h"
#include "debug_sys.h"
#include "debug_w25.h"
#include "external_flash_partitions.h"
#include "gpdma.h"
#include "gpioISR.h"
#include "memfault_platform_core.h"
#include "ncp_uart.h"
#include "nvmPartition.h"
#include "pca9535.h"
#include "pcap.h"
#include "printf.h"
#include "serial.h"
#include "serial_console.h"
#include "stm32_rtc.h"
#include "usb.h"
#include "w25.h"
#include "watchdog.h"

#include <stdio.h>
#include <string.h>

#define LED_BLUE LED_G
#define ALARM_OUT LED_R
#define USER_BUTTON BOOT

// LEDS are active low
#define LED_ON (0)
#define LED_OFF (1)


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

// Serial console (when no usb present)
SerialHandle_t usart3 = {
  .device = USART3,
  .name = "usart3",
  .txPin = &BM_MOSI_TX3,
  .rxPin = &BM_SCK_RX3,
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

extern "C" int main(void) {

    // Before doing anything, check if we should enter ROM bootloader
    // enterBootloaderIfNeeded();

    HAL_Init();

    SystemClock_Config();

    SystemPower_Config_ext();
    MX_GPIO_Init();
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

const char buttonTopic[] = "button";
const char on_str[] = "on";
const char off_str[] = "off";

bool buttonPress(const void *pinHandle, uint8_t value, void *args) {
    (void)pinHandle;
    (void)args;

    bm_pub_t publication;


    publication.topic = const_cast<char *>(buttonTopic);
    publication.topic_len = sizeof(buttonTopic) - 1; // Don't care about Null terminator

    if(value) {
        publication.data = const_cast<char *>(on_str);
        publication.data_len = sizeof(on_str) - 1; // Don't care about Null terminator
    } else {
        publication.data = const_cast<char *>(off_str);
        publication.data_len = sizeof(off_str) - 1; // Don't care about Null terminator
    }

    bm_pubsub_publish(&publication);

    return false;
}

void handle_sensor_subscriptions(const char* topic, uint16_t topic_len, const uint8_t* data, uint16_t data_len) {
    if (strncmp("button", topic, topic_len) == 0) {
        if (strncmp("on", reinterpret_cast<const char*>(data), data_len) == 0) {
            IOWrite(&LED_BLUE, LED_ON);
        } else if (strncmp("off", reinterpret_cast<const char*>(data), data_len) == 0) {
            IOWrite(&LED_BLUE, LED_OFF);
        } else {
            // Not handled
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

static void defaultTask( void *parameters ) {
    (void)parameters;

    startIWDGTask();
    startSerial();
    // Use USB for serial console if USB is connected on boot
    // Otherwise use ST-Link serial port
    if(usb_is_connected()) {
      startSerialConsole(&usbCLI);
      // Serial device will be enabled automatically when console connects
      // so no explicit serialEnable is required

      pcapInit(&usbPcap);

    } else {
      startSerialConsole(&usart1);
      serialEnable(&usart1);

      printf("WARNING: PCAP support requires USB connection.\n");
    }

    startCLI();
    // pcapInit(&usbPcap);

    gpioISRStartTask();

    memfault_platform_boot();
    memfault_platform_start();

    pca9535StartIRQTask();

    bspInit();

    usbInit(&VUSB_DETECT, usb_is_connected);

    debugSysInit();
    debugMemfaultInit(&usart1);

    debugGpioInit(debugGpioPins, sizeof(debugGpioPins)/sizeof(DebugGpio_t));
    debugBMInit();
    debugRTCInit();

    // // Re-enable low power mode
    // lpmPeripheralInactive(LPM_BOOT);

    gpioISRRegisterCallback(&USER_BUTTON, buttonPress);

    spiflash::W25 debugW25(&spi2, &FLASH_CS);
    debugW25Init(&debugW25);
    NvmPartition debug_cli_partition(debugW25, cli_configuration);
    NvmPartition dfu_partition(debugW25, dfu_configuration);
    debugNvmCliInit(&debug_cli_partition, &dfu_partition);
    debugDfuInit(&dfu_partition);
    bcl_init(&dfu_partition);
    ncpInit(&usart3);

    debug_ncp_init();

#ifdef BSP_BRIDGE_V1_0
#ifdef BRIDGE_AUTO_ENABLE
    printf("Enabling 24V!\n");
    IOWrite(&BOOST_EN, 1);
    IOWrite(&VBUS_SW_EN, 1);
#endif
#endif

    IOWrite(&ALARM_OUT, 1);
    IOWrite(&LED_BLUE, LED_OFF);

    bm_sub_t subscription;

    subscription.topic = const_cast<char *>(buttonTopic);
    subscription.topic_len = sizeof(buttonTopic) - 1; // Don't care about Null terminator
    subscription.cb = handle_sensor_subscriptions;
    bm_pubsub_subscribe(&subscription);

    while(1) {
        /* Do nothing */
        vTaskDelay(1000);
    }
}
