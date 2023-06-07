
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
#include "debug_gpio.h"
#include "debug_memfault.h"
#include "debug_rtc.h"
#include "debug_sys.h"
#include "gpdma.h"
#include "gpioISR.h"
#include "memfault_platform_core.h"
#include "pca9535.h"
#include "pcap.h"
#include "printf.h"
#include "serial.h"
#include "serial_console.h"
#include "stm32_rtc.h"
#include "stress.h"
#include "usb.h"
#include "watchdog.h"
#include "timer_callback_handler.h"
#ifndef BSP_NUCLEO_U575
#include "w25.h"
#include "debug_w25.h"
#include "nvmPartition.h"
#include "external_flash_partitions.h"
#include "debug_nvm_cli.h"
#include "debug_dfu.h"
#include "bridgePowerController.h"
#include "debug_bridge_power_controller.h"
#include "debug_configuration.h"
#include "ram_partitions.h"
#endif


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

#ifndef BSP_MOTE_V1_0
extern "C" void USART1_IRQHandler(void) {
    serialGenericUartIRQHandler(&usart1);
}
#endif // BM_MOTE_V1_0

extern "C" int main(void) {

    // Before doing anything, check if we should enter ROM bootloader
    // enterBootloaderIfNeeded();

    HAL_Init();

    SystemClock_Config();

    SystemPower_Config_ext();
    MX_GPIO_Init();
#ifndef BSP_MOTE_V1_0
    MX_USART1_UART_Init();
#endif // BM_MOTE_V1_0
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

bool start_stress(const void *pinHandle, uint8_t value, void *args) {
    (void)pinHandle;
    (void)args;
    static bool started;

    if(!started && value) {
        started = true;
        stress_start_tx((2 * 1024)/8);
    }

    return false;
}

const char buttonTopic[] = "button";
const char on_str[] = "on";
const char off_str[] = "off";

bool buttonPress(const void *pinHandle, uint8_t value, void *args) {
    (void)pinHandle;
    (void)args;

    if(value) {
        bm_pub(buttonTopic, on_str, sizeof(on_str) - 1);
    } else {
        bm_pub(buttonTopic, off_str, sizeof(off_str) - 1);
    }

    return false;
}

void handle_sensor_subscriptions(uint64_t node_id, const char* topic, uint16_t topic_len, const uint8_t* data, uint16_t data_len) {
    (void)node_id;
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

#ifdef BSP_BRIDGE_V1_0
    debugGpioInit(debugGpioPins, sizeof(debugGpioPins)/sizeof(DebugGpio_t));
#endif
    debugBMInit();
    debugRTCInit();
    timer_callback_handler_init();
    // Disabling now for hard mode testing
    // Re-enable low power mode
    // lpmPeripheralInactive(LPM_BOOT);

    gpioISRRegisterCallback(&USER_BUTTON, buttonPress);
#ifdef BSP_DEV_MOTE_V1_0
    gpioISRRegisterCallback(&BOOT_LED, start_stress);
#endif
#ifndef BSP_NUCLEO_U575
    spiflash::W25 debugW25(&spi2, &FLASH_CS);
    debugW25Init(&debugW25);
    NvmPartition debug_user_partition(debugW25, user_configuration);
    NvmPartition debug_hardware_partition(debugW25, hardware_configuration);
    NvmPartition debug_system_partition(debugW25, system_configuration);
    cfg::Configuration debug_configuration_user(debug_user_partition,ram_user_configuration, RAM_USER_CONFIG_SIZE_BYTES);
    cfg::Configuration debug_configuration_hardware(debug_hardware_partition,ram_hardware_configuration, RAM_HARDWARE_CONFIG_SIZE_BYTES);
    cfg::Configuration debug_configuration_system(debug_system_partition,ram_system_configuration, RAM_SYSTEM_CONFIG_SIZE_BYTES);
    NvmPartition debug_cli_partition(debugW25, cli_configuration);
    NvmPartition dfu_partition(debugW25, dfu_configuration);
    debugConfigurationInit(&debug_configuration_user,&debug_configuration_hardware,&debug_configuration_system);
    debugNvmCliInit(&debug_cli_partition, &dfu_partition);
    debugDfuInit(&dfu_partition);
    bcl_init(&dfu_partition,&debug_configuration_user, &debug_configuration_system);
#else
    bcl_init(NULL, NULL, NULL);
#endif

#ifdef BSP_BRIDGE_V1_0
#ifdef BRIDGE_AUTO_ENABLE
    printf("Enabling 24V!\n");
    IOWrite(&BOOST_EN, 1);
    IOWrite(&VBUS_SW_EN, 1);
#else
    uint32_t sampleIntervalMs = BridgePowerController::DEFAULT_SAMPLE_INTERVAL_MS;
    debug_configuration_system.getConfig("sampleIntervalMs",strlen("sampleIntervalMs"),sampleIntervalMs);
    uint32_t sampleDurationMs = BridgePowerController::DEFAULT_SAMPLE_DURATION_MS;
    debug_configuration_system.getConfig("sampleDurationMs",strlen("sampleDurationMs"),sampleDurationMs);
    uint32_t subSampleIntervalMs = BridgePowerController::DEFAULT_SUBSAMPLE_INTERVAL_MS;
    debug_configuration_system.getConfig("subSampleIntervalMs",strlen("subSampleIntervalMs"),subSampleIntervalMs);
    uint32_t subsampleDurationMs = BridgePowerController::DEFAULT_SUBSAMPLE_DURATION_MS;
    debug_configuration_system.getConfig("subsampleDurationMs",strlen("subsampleDurationMs"),subsampleDurationMs);
    uint32_t subsampleEnabled = 0;
    debug_configuration_system.getConfig("subsampleEnabled",strlen("subsampleEnabled"),subsampleEnabled);
    uint32_t bridgePowerControllerEnabled = 0;
    debug_configuration_system.getConfig("bridgePowerControllerEnabled", strlen("bridgePowerControllerEnabled"), bridgePowerControllerEnabled);
    printf("Using bridge power controller.\n");
    IOWrite(&BOOST_EN, 1);
    BridgePowerController bridge_power_controller(VBUS_SW_EN, sampleIntervalMs,
        sampleDurationMs, subSampleIntervalMs, subsampleDurationMs, static_cast<bool>(subsampleEnabled), static_cast<bool>(bridgePowerControllerEnabled));
#endif
#endif

    IOWrite(&ALARM_OUT, 1);
    IOWrite(&LED_BLUE, LED_OFF);
#ifdef BSP_DEV_MOTE_V1_0
    IOWrite(&EXP_LED_G2, LED_OFF);
    IOWrite(&EXP_LED_R1, LED_OFF);
#endif // BSP_DEV_MOTE_V1_0

    bm_sub(buttonTopic, handle_sensor_subscriptions);

    while(1) {
        /* Do nothing */
        vTaskDelay(1000);
    }
}
