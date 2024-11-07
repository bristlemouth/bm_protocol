// Includes for FreeRTOS
#include "FreeRTOS.h"
#include "configuration.h"
#include "device_info.h"
#include "dfu.h"
#include "messages.h"
#include "queue.h"
#include "task.h"

#include "bm_ports.h"
#include "bsp.h"

extern "C" {
#include "bcmp.h"
#include "bm_adin2111.h"
#include "bm_ip.h"
#include "bm_service.h"
#include "device.h"
#include "l2.h"
#include "middleware.h"
}
#include "bcmp_cli.h"

#include "task_priorities.h"

#include "bm_config.h"

#ifdef STRESS_TEST_ENABLE
#include "stress.h"
#endif

static void adin_power_callback(bool on) { IOWrite(&ADIN_PWR, on); }

void bcl_init(void) {
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

  NetworkDevice network_device = adin2111_network_device();
  network_device.callbacks->power = adin_power_callback;
  BmErr err = adin2111_init();
  if (err != BmOK) {
    printf("ADIN2111 init error %d\n", err);
  }

  bm_l2_init(network_device);
  bm_ip_init();
  bcmp_init(network_device);
  bcmp_cli_init();

  // TODO: move this init to middle_ware init once services have been ported
  bm_service_init();
  bm_middleware_init(BM_MIDDLEWARE_PORT);

#ifdef STRESS_TEST_ENABLE
  stress_test_init(&netif, STRESS_TEST_PORT);
#endif
}

/*!
  @brief Get string representation of IP address.

  @param[in] ip IP address index
  @return pointer to string with ip address
*/
const char *bcl_get_ip_str(uint8_t idx) { return bm_ip_get_str(idx); }
