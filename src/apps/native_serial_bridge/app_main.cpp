// Includes from CubeMX Generated files
#include "main.h"

// Peripheral
#include "gpio.h"
#include "icache.h"
#include "iwdg.h"
#include "usart.h"
#include "usb_otg.h"

// Includes for FreeRTOS
#include "FreeRTOS.h"
#include "task.h"
#include "task_priorities.h"

#include "bcmp_cli.h"
#include "bm_adin2111.h"
#include "bm_config.h"
extern "C" {
#include "bm_ip.h"
}
#include "bristlefin.h"
#include "bristlemouth_client.h"
#include "bsp.h"
#include "cli.h"
#include "composite_device.h"
#include "config_cbor_map_service.h"
#include "configuration.h"
#include "debug_bm_service.h"
#include "debug_configuration.h"
#include "debug_dfu.h"
#include "debug_gpio.h"
#include "debug_memfault.h"
#include "debug_nvm_cli.h"
#include "debug_rtc.h"
#include "debug_spotter.h"
#include "debug_sys.h"
#include "debug_w25.h"
#include "device_info.h"
#include "external_flash_partitions.h"
#include "gpdma.h"
#include "gpioISR.h"
#include "l2.h"
#include "memfault_platform_core.h"
#include "nvmPartition.h"
#include "pcap.h"
#include "port_monitor.h"
#include "printf.h"
#include "ram_partitions.h"
#include "sensorSampler.h"
#include "sensors.h"
#include "serial.h"
#include "serial_console.h"
#include "stm32_rtc.h"
#include "sys_info_service.h"
#include "timer_callback_handler.h"
#include "uart_network_device.h"
#include "uptime.h"
#include "usb.h"
#include "w25.h"
#include "watchdog.h"

#define LED_ON_TIME_MS 20
#define LED_PERIOD_MS 1000

extern "C" {
#include "bcmp.h"
#include "bm_service.h"
#include "bristlemouth.h"
#include "device.h"
#include "middleware.h"
#include "pubsub.h"
#include "topology.h"
}

#include <stdio.h>
#include <string.h>

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
};

extern "C" void USART3_IRQHandler(void) { serialGenericUartIRQHandler(&usart3); }

// TODO - make a getter API for these
NvmPartition *userConfigurationPartition = NULL;
NvmPartition *systemConfigurationPartition = NULL;
NvmPartition *hardwareConfigurationPartition = NULL;
NvmPartition *dfu_partition_global = NULL;

// ---------------------------------------------------------------------------
// ADIN2111 interrupt → L2
// ---------------------------------------------------------------------------

static bool network_device_interrupt(const void *pinHandle, uint8_t value, void *args) {
  (void)pinHandle;
  (void)value;
  (void)args;
  return bm_l2_handle_device_interrupt() == BmOK;
}

// ---------------------------------------------------------------------------
// ADIN2111 power / reset callback (used as the composite power callback)
// ---------------------------------------------------------------------------

#define RESET_DELAY (1)
#define AFTER_RESET_DELAY (100)

static void native_power_callback(bool on) {
  IOWrite(&ADIN_PWR, on);
  if (on) {
    IOWrite(&ADIN_CS, 1);
    IOWrite(&ADIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(RESET_DELAY));
    IOWrite(&ADIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(AFTER_RESET_DELAY));
  }
}

// ---------------------------------------------------------------------------
// Debug GPIO pin table (Bristleback mote pins)
// ---------------------------------------------------------------------------

// TODO - move this to some debug file?
static const DebugGpio_t debugGpioPins[] = {
    {"adin_cs", &ADIN_CS, GPIO_OUT},        {"adin_int", &ADIN_INT, GPIO_IN},
    {"adin_pwr", &ADIN_PWR, GPIO_OUT},      {"adin_rst", &ADIN_RST, GPIO_OUT},
    {"flash_cs", &FLASH_CS, GPIO_OUT},      {"boot_led", &BOOT_LED, GPIO_IN},
    {"vusb_detect", &VUSB_DETECT, GPIO_IN},
};

// ---------------------------------------------------------------------------
// main() — minimal pre-scheduler setup
// ---------------------------------------------------------------------------

extern "C" int main(void) {
  HAL_Init();

  SystemClock_Config();

  SystemPower_Config_ext();

  // Enable hardfault on divide-by-zero
  SCB->CCR |= 0x10;

  BaseType_t rval = xTaskCreate(defaultTask, "Default",
                                // TODO - verify stack size
                                128 * 4, NULL,
                                // Start with very high priority during boot
                                // then downgrade once done initializing
                                DEFAULT_BOOT_TASK_PRIORITY, NULL);
  configASSERT(rval == pdTRUE);

  // Start FreeRTOS scheduler
  vTaskStartScheduler();

  /* We should never get here as control is now taken by the scheduler */
  while (1) {
  }
}

// ---------------------------------------------------------------------------
// defaultTask — all initialization happens here (post-scheduler)
// ---------------------------------------------------------------------------

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

  bspInit();
  usbInit(&VUSB_DETECT, usb_is_connected);

  debugSysInit();
  debugMemfaultInit(&usbCLI);

  debugGpioInit(debugGpioPins, sizeof(debugGpioPins) / sizeof(DebugGpio_t));
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

  // -----------------------------------------------------------------------
  // Bristlemouth stack init — manual sequence bypassing bcl_init() /
  // bristlemouth_init() so we can inject our 3-port composite device.
  // -----------------------------------------------------------------------

  // Register the ADIN2111 GPIO interrupt → L2 before enabling the device.
  extern adin_pins_t adin_pins;
  IORegisterCallback(adin_pins.interrupt, network_device_interrupt, NULL);

  HAL_Init_Hook();
  config_init();
  port_monitor_init();

  uint8_t major, minor, patch;
  getFWVersion(&major, &minor, &patch);
  DeviceCfg device_cfg = {
      .node_id = getNodeId(),
      .git_sha = getGitSHA(),
      .device_name = getUIDStr(),
      .version_string = getFWVersionStr(),
      .vendor_id = 0,
      .product_id = 0,
      .hw_ver = 0,
      .ver_major = major,
      .ver_minor = minor,
      .ver_patch = patch,
      .sn = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'},
  };
  device_init(device_cfg);

  // Build the composite 3-port network device:
  //   Port 1 → ADIN port 1
  //   Port 2 → ADIN port 2
  //   Port 3 → UART (PLUART) port 1
  NetworkDevice adin_dev = adin2111_network_device();
  NetworkDevice uart_dev = uart_network_device(UART_RX_TASK_PRIORITY);
  NetworkDevice composite_dev = composite_network_device(adin_dev, uart_dev);

  // Wire the ADIN power/reset callback through the composite power callback.
  composite_dev.callbacks->power = native_power_callback;

  printf("Starting native_serial_bridge BM stack (3-port composite)\n");

  BmErr err = BmOK;
  bm_err_check(err, adin2111_init());
  bm_err_check(err, bm_l2_init(composite_dev));
  bm_err_check(err, bm_ip_init());
  bm_err_check(err, bcmp_init(composite_dev));
  bm_err_check(err, topology_init(composite_dev.trait->num_ports()));
  bm_err_check(err, bm_service_init());
  bm_err_check(err, bm_pubsub_init());
  bm_err_check(err, bm_middleware_init());

  if (err == BmOK) {
    // Enable packet capture output over USB VCP.
    composite_dev.callbacks->debug_packet_dump = pcapTxPacket;
    bcmp_cli_init();
  } else {
    printf("ERROR: BM stack init failed (%d)\n", err);
  }

  debugConfigurationInit();

  uint32_t sys_cfg_sensorsPollIntervalMs = DEFAULT_SENSORS_POLL_MS;
  uint32_t sys_cfg_sensorsCheckIntervalS = DEFAULT_SENSORS_CHECK_S;

  get_config_uint(BM_CFG_PARTITION_SYSTEM, "sensorsPollIntervalMs",
                  strlen("sensorsPollIntervalMs"), &sys_cfg_sensorsPollIntervalMs);
  get_config_uint(BM_CFG_PARTITION_SYSTEM, "sensorsCheckIntervalS",
                  strlen("sensorsCheckIntervalS"), &sys_cfg_sensorsCheckIntervalS);

  sensorConfig_t sensorConfig = {.sensorCheckIntervalS = sys_cfg_sensorsCheckIntervalS,
                                 .sensorsPollIntervalMs = sys_cfg_sensorsPollIntervalMs};
  sensorSamplerInit(&sensorConfig);
  sensorsInit();
  debugBmServiceInit();
  sys_info_service_init();
  config_cbor_map_service_init();

  // Re-enable low power mode
  lpmPeripheralInactive(LPM_BOOT);

  // Drop priority now that we're done booting
  vTaskPrioritySet(xTaskGetCurrentTaskHandle(), DEFAULT_TASK_PRIORITY);

  // 1 Hz heartbeat: LED1 green on for LED_ON_TIME_MS, off for the remainder.
  bool ledState = false;
  uint64_t ledLastScheduledOnTime = uptimeGetMs();

  while (1) {
    const uint64_t elapsedSinceOnTime = uptimeGetMs() - ledLastScheduledOnTime;

    if (!ledState && elapsedSinceOnTime >= LED_PERIOD_MS) {
      ledLastScheduledOnTime += LED_PERIOD_MS;
      bristlefin.setLed(1, Bristlefin::LED_GREEN);
      ledState = true;
    } else if (ledState && elapsedSinceOnTime >= LED_ON_TIME_MS) {
      bristlefin.setLed(1, Bristlefin::LED_OFF);
      ledState = false;
    }

    vTaskDelay(pdMS_TO_TICKS(1));
  }
}
