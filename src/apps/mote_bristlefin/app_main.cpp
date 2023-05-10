
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
#include "debug_tca9546a.h"
#include "gpdma.h"
#include "gpioISR.h"
#include "memfault_platform_core.h"
#include "ms5803.h"
#include "pca9535.h"
#include "pcap.h"
#include "printf.h"
#include "sensors.h"
#include "sensorSampler.h"
#include "serial.h"
#include "serial_console.h"
#include "stm32_rtc.h"
#include "stress.h"
#include "tca9546a.h"
#include "usb.h"
#include "watchdog.h"
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

#define LED_ON (0)
#define LED_OFF (1)

#include <stdio.h>
#include <string.h>

static void defaultTask(void *parameters);

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

extern "C" void USART3_IRQHandler(void) {
    serialGenericUartIRQHandler(&usart3);
}

static TCA::TCA9546A bristlefinTCA(&i2c1, TCA9546A_ADDR, &I2C_MUX_RESET);

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
            IOWrite(&BF_LED_G1, LED_ON);
        } else if (strncmp("off", reinterpret_cast<const char*>(data), data_len) == 0) {
            IOWrite(&BF_LED_G1, LED_OFF);
        } else {
            // Not handled
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
  {"adin_rst", &ADIN_RST, GPIO_OUT},
  {"i2c_mux_rst", &I2C_MUX_RESET, GPIO_OUT},
  {"gpio1", &GPIO1, GPIO_OUT},
  {"gpio2", &GPIO2, GPIO_OUT},
  {"bm_int", &BM_INT, GPIO_IN},
  {"bm_cs", &BM_CS, GPIO_OUT},
  {"vbus_bf_en", &VBUS_BF_EN, GPIO_OUT},
  {"flash_cs", &FLASH_CS, GPIO_OUT},
  {"boot_led", &BOOT_LED, GPIO_IN},
  {"vusb_detect", &VUSB_DETECT, GPIO_IN},
  {"bf_io1", &BF_IO1, GPIO_OUT},
  {"bf_io2", &BF_IO2, GPIO_OUT},
  {"bf_hfio", &BF_HFIO, GPIO_OUT},
  {"bf_3v3_en", &BF_3V3_EN, GPIO_IN},
  {"bf_5v_en", &BF_5V_EN, GPIO_OUT},
  {"bf_imu_int", &BF_IMU_INT, GPIO_IN},
  {"bf_imu_rst", &BF_IMU_RST, GPIO_OUT},
  {"bf_sdi12_oe", &BF_SDI12_OE, GPIO_OUT},
  {"bf_tp16", &BF_TP16, GPIO_OUT},
  {"bf_led_g1", &BF_LED_G1, GPIO_OUT},
  {"bf_led_r1", &BF_LED_R1, GPIO_OUT},
  {"bf_led_g2", &BF_LED_G2, GPIO_OUT},
  {"bf_led_r2", &BF_LED_R2, GPIO_OUT},
  {"bf_pl_buck_en", &BF_PL_BUCK_EN, GPIO_OUT},
  {"bf_tp7", &BF_TP7, GPIO_OUT},
  {"bf_tp8", &BF_TP8, GPIO_OUT},
};

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
      startSerialConsole(&usart3);
      serialEnable(&usart3);

#if BM_DFU_HOST
      printf("WARNING: USB must be connected to use DFU mode. This serial port will be used by the serial console instead.\n");
#endif

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

    if(bristlefinTCA.init()){
        bristlefinTCA.setChannel(TCA::CH_1);
    }

    debugSysInit();
    if(usb_is_connected()) {
        debugMemfaultInit(&usbCLI);
    } else {
        debugMemfaultInit(&usart3);
    }

    debugGpioInit(debugGpioPins, sizeof(debugGpioPins)/sizeof(DebugGpio_t));
    debugBMInit();
    debugRTCInit();
    debugTCA9546AInit(&bristlefinTCA);

    // Disabling now for hard mode testing
    // Re-enable low power mode
    // lpmPeripheralInactive(LPM_BOOT);

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
    bcl_init(&dfu_partition);

    sensorsInit();
    // TODO - get this from the nvm cfg's!
    sensorConfig_t sensorConfig = { .sensorCheckIntervalS=10 };
    sensorSamplerInit(&sensorConfig);

    bm_sub_t subscription;

    subscription.topic = const_cast<char *>(buttonTopic);
    subscription.topic_len = sizeof(buttonTopic) - 1; // Don't care about Null terminator
    subscription.cb = handle_sensor_subscriptions;
    bm_pubsub_subscribe(&subscription);

    // Turn of the bristlefin leds
    IOWrite(&BF_LED_G1, LED_OFF);
    IOWrite(&BF_LED_R1, LED_OFF);
    IOWrite(&BF_LED_G2, LED_OFF);
    IOWrite(&BF_LED_R2, LED_OFF);

    while(1) {
        /* Do nothing */
        vTaskDelay(1000);
    }
}
