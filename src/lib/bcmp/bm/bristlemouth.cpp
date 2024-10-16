// Includes for FreeRTOS
#include "FreeRTOS.h"
#include "device_info.h"
#include "dfu/dfu.h"
#include "messages.h"
#include "queue.h"
#include "task.h"

#include "bm_ports.h"

extern "C" {
#include "bcmp.h"
#include "bm_ip.h"
#include "device.h"
#include "eth_adin2111.h"
}
#include "bcmp_cli.h"

#include "middleware.h"
#include "task_priorities.h"

#include "bm_config.h"

#ifdef STRESS_TEST_ENABLE
#include "stress.h"
#endif

struct netif netif;

// Callback function in case of link changes.
// Will notify relevant subsystems of link change event
void bm_link_change_cb(uint8_t port, bool state) {
  printf("bm port%u %s\n", port, state ? "up" : "down");

  // Let BCMP know a link has changed!
  // (Useful for heartbeats/discovery)
  bcmp_link_change(port, state);
}

void bcl_init(void) {
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

  // TODO: this is specific configuration to the netif driver.
  // this should be placed elsewhere based on the users chosen 10BASE-T1L
  // chipset selection
  static adin2111_DeviceStruct_t adin_device;
  static const BmNetDevCfg bm_netdev_config[] = {
      {
          &adin_device,
          ADIN_PORT_MASK_ALL,
          ADIN2111_PORT_NUM,
          adin2111_hw_init,
          adin2111_power_cb,
          adin2111_tx,
      },
      {
          NULL,
          0,
          0,
          NULL,
          NULL,
          NULL,
      },
  };

  bm_l2_init(bm_link_change_cb, bm_netdev_config, array_size(bm_netdev_config));

  bm_ip_init();

  bcmp_init();
  //TODO: These items need to be moved into bcmp again once we figure out how
  //      to get partitions/configurations configurable by the user
  //bm_dfu_init(bcmp_dfu_tx, dfu_partition, sys_cfg);

  bcmp_cli_init();

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
