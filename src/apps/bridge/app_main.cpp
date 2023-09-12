
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
#include "debug_spotter.h"
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
#include "bm_serial.h"
#include "ncp_uart.h"
#include "nvmPartition.h"
#include "pca9535.h"
#include "pcap.h"
#include "printf.h"
#include "serial.h"
#include "serial_console.h"
#include "stm32_rtc.h"
#include "topology_sampler.h"
#include "usb.h"
#include "w25.h"
#include "watchdog.h"
#include "configuration.h"
#include "debug_configuration.h"
#include "ram_partitions.h"
#include "bridgePowerController.h"
#include "debug_bridge_power_controller.h"
#include "timer_callback_handler.h"
#include "util.h"
#include "app_pub_sub.h"
#include "bcmp_neighbors.h"
#include "bridgeLog.h"
#ifdef USE_MICROPYTHON
#include "micropython_freertos.h"
#endif

#include <stdio.h>
#include <string.h>
#include <stdint.h>

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
SerialHandle_t usbCLI   = {
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

SerialHandle_t usbPcap   = {
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

bool buttonPress(const void *pinHandle, uint8_t value, void *args) {
    (void)pinHandle;
    (void)args;

    if(value) {
        bm_pub(APP_PUB_SUB_BUTTON_TOPIC, APP_PUB_SUB_BUTTON_CMD_ON, sizeof(APP_PUB_SUB_BUTTON_CMD_ON) - 1, APP_PUB_SUB_BUTTON_TYPE);
    } else {
        bm_pub(APP_PUB_SUB_BUTTON_TOPIC, APP_PUB_SUB_BUTTON_CMD_OFF, sizeof(APP_PUB_SUB_BUTTON_CMD_OFF) - 1, APP_PUB_SUB_BUTTON_TYPE);
    }

    return false;
}

static void handle_subscriptions(uint64_t node_id, const char* topic, uint16_t topic_len, const uint8_t* data, uint16_t data_len, uint8_t type, uint8_t version) {
    (void)node_id;
    if (strncmp(APP_PUB_SUB_BUTTON_TOPIC, topic, topic_len) == 0) {
        if(type == APP_PUB_SUB_BUTTON_TYPE && version == APP_PUB_SUB_BUTTON_VERSION){
            if (strncmp(APP_PUB_SUB_BUTTON_CMD_ON, reinterpret_cast<const char*>(data), data_len) == 0) {
                IOWrite(&LED_BLUE, LED_ON);
            } else if (strncmp(APP_PUB_SUB_BUTTON_CMD_OFF, reinterpret_cast<const char*>(data), data_len) == 0) {
                IOWrite(&LED_BLUE, LED_OFF);
            } else {
                // Not handled
            }
        } else {
            printf("Unrecognized version: %u and type: %u\n", version, type);
        }
    } else if(strncmp(APP_PUB_SUB_PRINTF_TOPIC, topic, topic_len) == 0){
        if(type == APP_PUB_SUB_PRINTF_TYPE && version == APP_PUB_SUB_PRINTF_VERSION){
            printf("Topic: %.*s\n", topic_len, topic);
            printf("Data: %.*s\n", data_len, data);
            if(bm_serial_tx(BM_SERIAL_LOG, const_cast<uint8_t*>(data), data_len) != 0){
                printf("Failed to forward to NCP.\n");
            }
        } else {
            printf("Unrecognized version: %u and type: %u\n", version, type);
        }
    } else if(strncmp(APP_PUB_SUB_UTC_TOPIC, topic, topic_len) == 0) {
        if(type == APP_PUB_SUB_UTC_TYPE && version == APP_PUB_SUB_UTC_VERSION){
            utcDateTime_t time;
            const bm_common_pub_sub_utc_t *utc = reinterpret_cast<const bm_common_pub_sub_utc_t *>(data);
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
                printf("Updating RTC to %u-%u-%u %02u:%02u:%02u.%04u\n",
                    rtc_time.year,
                    rtc_time.month,
                    rtc_time.day,
                    rtc_time.hour,
                    rtc_time.minute,
                    rtc_time.second,
                    rtc_time.ms);
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

static void neighborDiscoveredCb(bool discovered, bm_neighbor_t *neighbor) {
    configASSERT(neighbor);
    static constexpr size_t bufsize = 50;
    const char * action = (discovered) ? "added" : "lost";
    char buffer[bufsize];
    size_t len = snprintf(buffer, bufsize, "Neighbor %" PRIx64 " %s\n", neighbor->node_id, action);
    BRIDGE_LOG_PRINTN(buffer, len);
}

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

    usbInit(&VUSB_DETECT, usb_is_connected);

    debugSysInit();
    debugMemfaultInit(&usbCLI);

    debugGpioInit(debugGpioPins, sizeof(debugGpioPins)/sizeof(DebugGpio_t));
    debugSpotterInit();
    debugRTCInit();

    timer_callback_handler_init();

    gpioISRRegisterCallback(&USER_BUTTON, buttonPress);

    spiflash::W25 debugW25(&spi2, &FLASH_CS);
    debugW25Init(&debugW25);
    NvmPartition debug_user_partition(debugW25, user_configuration);
    NvmPartition debug_hardware_partition(debugW25, hardware_configuration);
    NvmPartition debug_system_partition(debugW25, system_configuration);
    cfg::Configuration debug_configuration_user(debug_user_partition,ram_user_configuration, RAM_USER_CONFIG_SIZE_BYTES);
    cfg::Configuration debug_configuration_hardware(debug_hardware_partition,ram_hardware_configuration, RAM_HARDWARE_CONFIG_SIZE_BYTES);
    cfg::Configuration debug_configuration_system(debug_system_partition,ram_system_configuration, RAM_SYSTEM_CONFIG_SIZE_BYTES);
    debugConfigurationInit(&debug_configuration_user,&debug_configuration_hardware,&debug_configuration_system);
    NvmPartition debug_cli_partition(debugW25, cli_configuration);
    NvmPartition dfu_partition(debugW25, dfu_configuration);
    debugNvmCliInit(&debug_cli_partition, &dfu_partition);
    debugDfuInit(&dfu_partition);
    bcl_init(&dfu_partition,&debug_configuration_user, &debug_configuration_system);

    uint32_t sampleIntervalMs = BridgePowerController::DEFAULT_SAMPLE_INTERVAL_S * 1000;
    debug_configuration_system.getConfig("sampleIntervalMs", strlen("sampleIntervalMs"),sampleIntervalMs);
    uint32_t sampleDurationMs = BridgePowerController::DEFAULT_SAMPLE_DURATION_S * 1000;
    debug_configuration_system.getConfig("sampleDurationMs",strlen("sampleDurationMs"),sampleDurationMs);
    uint32_t subSampleIntervalMs = BridgePowerController::DEFAULT_SUBSAMPLE_INTERVAL_S * 1000;
    debug_configuration_system.getConfig("subSampleIntervalMs",strlen("subSampleIntervalMs"),subSampleIntervalMs);
    uint32_t subsampleDurationMs = BridgePowerController::DEFAULT_SUBSAMPLE_DURATION_S * 1000;
    debug_configuration_system.getConfig("subsampleDurationMs",strlen("subsampleDurationMs"),subsampleDurationMs);
    uint32_t subsampleEnabled = BridgePowerController::DEFAULT_SUBSAMPLE_ENABLED;
    debug_configuration_system.getConfig("subsampleEnabled", strlen("subsampleEnabled"), subsampleEnabled);
    uint32_t bridgePowerControllerEnabled = BridgePowerController::DEFAULT_POWER_CONTROLLER_ENABLED;
    debug_configuration_system.getConfig("bridgePowerControllerEnabled", strlen("bridgePowerControllerEnabled"), bridgePowerControllerEnabled);
    uint32_t alignmentInterval5Min = BridgePowerController::DEFAULT_ALIGNMENT_5_MIN_INTERVAL;    
    debug_configuration_system.getConfig("alignmentInterval5Min", strlen("bridgePowerControllerEnabled"), alignmentInterval5Min);
    printf("Using bridge power controller.\n");
    IOWrite(&BOOST_EN, 1);
    BridgePowerController bridge_power_controller(VBUS_SW_EN, sampleIntervalMs,
        sampleDurationMs, subSampleIntervalMs, subsampleDurationMs, static_cast<bool>(subsampleEnabled), static_cast<bool>(bridgePowerControllerEnabled), (alignmentInterval5Min * BridgePowerController::DEFAULT_ALIGNMENT_S));
    ncpInit(&usart3, &dfu_partition, &bridge_power_controller, &debug_configuration_user, &debug_configuration_system, &debug_configuration_hardware);
    topology_sampler_init(&bridge_power_controller, &debug_configuration_hardware, &debug_configuration_system);
    debug_ncp_init();

    IOWrite(&ALARM_OUT, 1);
    IOWrite(&LED_BLUE, LED_OFF);

    bm_sub(APP_PUB_SUB_BUTTON_TOPIC, handle_subscriptions);
    bm_sub(APP_PUB_SUB_PRINTF_TOPIC, handle_subscriptions);
    bm_sub(APP_PUB_SUB_UTC_TOPIC, handle_subscriptions);
    bcmp_neighbor_register_discovery_callback(neighborDiscoveredCb);

#ifdef USE_MICROPYTHON
    micropython_freertos_init(&usbCLI);
#endif

    // // Re-enable low power mode
    lpmPeripheralInactive(LPM_BOOT);

    while(1) {
        /* Do nothing */
        vTaskDelay(1000);
    }
}
