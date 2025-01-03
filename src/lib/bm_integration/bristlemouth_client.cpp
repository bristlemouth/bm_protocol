#include "bristlemouth_client.h"

// Includes for FreeRTOS
#include "FreeRTOS.h"
#include "configuration.h"
#include "device_info.h"
#include "dfu.h"
#include "messages.h"
#include "queue.h"
#include "task.h"

#include "adi_hal.h"
#include "bcmp_cli.h"
#include "bm_ports.h"
#include "bsp.h"
#include "l2.h"
#include "pcap.h"
#include "task_priorities.h"

extern "C" {
#include "bm_ip.h"
#include "bristlemouth.h"
#include "device.h"
}

#ifdef STRESS_TEST_ENABLE
#include "stress.h"
#endif

#define RESET_DELAY (1)
#define AFTER_RESET_DELAY (100)

extern adin_pins_t adin_pins;

static bool network_device_interrupt(const void *pinHandle, uint8_t value, void *args) {
  (void)pinHandle;
  (void)value;
  (void)args;
  return bm_l2_handle_device_interrupt() == BmOK;
}

static void adin_power_callback(bool on) {
  IOWrite(&ADIN_PWR, on);
  if (on) {
    IOWrite(&ADIN_CS, 1);
    IOWrite(&ADIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(RESET_DELAY));
    IOWrite(&ADIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(AFTER_RESET_DELAY));
  }
}

void bcl_init(void) {
  IORegisterCallback(adin_pins.interrupt, network_device_interrupt, NULL);
  HAL_Init_Hook();
  config_init();
  //TODO: This should all be moved to user code
  uint8_t major, minor, patch;
  getFWVersion(&major, &minor, &patch);
  DeviceCfg device = {
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

  printf("Starting up BCL\n");
  device_init(device);

  BmErr err = bristlemouth_init(adin_power_callback);
  if (err == BmOK) {
    NetworkDevice network_device = bristlemouth_network_device();
    network_device.callbacks->debug_packet_dump = pcapTxPacket;
    bcmp_cli_init();

#ifdef STRESS_TEST_ENABLE
    stress_test_init(network_device, STRESS_TEST_PORT);
#endif

  } else {
    printf("Failed to initialize bristlemouth\n");
  }
}

/*!
  @brief Get string representation of IP address.

  @param[in] ip IP address index
  @return pointer to string with ip address
*/
const char *bcl_get_ip_str(uint8_t idx) { return bm_ip_get_str(idx); }
