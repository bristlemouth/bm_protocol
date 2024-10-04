#include "bristlemouth.h"

// Includes for FreeRTOS
#include "FreeRTOS.h"
#include "device_info.h"
#include "queue.h"
#include "task.h"

#include "lwip/inet.h"
#include "lwip/init.h"
#include "lwip/mld6.h"
#include "lwip/tcpip.h"
#include "lwip/udp.h"

#include "bm_l2.h"
#include "bm_ports.h"

#include "bcmp.h"
#include "bcmp_cli.h"

#include "middleware.h"
#include "task_priorities.h"

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

void bcl_init(NvmPartition *dfu_partition) {
  err_t mld6_err;
  // int         rval;
  ip6_addr_t multicast_glob_addr;

  printf("Starting up BCL\n");

  /* Looks like we don't call lwip_init if we are using a RTOS */
  tcpip_init(NULL, NULL);

  getMacAddr(netif.hwaddr, sizeof(netif.hwaddr));
  netif.hwaddr_len = sizeof(netif.hwaddr);

  inet6_aton("ff03::1", &multicast_glob_addr);

  // Initialize l2 structs/callbacks
  bm_l2_init(bm_link_change_cb);

  // The documentation says to use tcpip_input if we are running with an OS
  // bm_l2_netif_init will be called from netif_add
  netif_add(&netif, NULL, bm_l2_netif_init, tcpip_input);

  // Create IP addresses from node_id
  const uint64_t node_id = getNodeId();
  ip_addr_t unicast_addr =
      IPADDR6_INIT_HOST(0xFD000000, 0, (node_id >> 32) & 0xFFFFFFFF, node_id & 0xFFFFFFFF);

  ip_addr_t ll_addr =
      IPADDR6_INIT_HOST(0xFE800000, 0, (node_id >> 32) & 0xFFFFFFFF, node_id & 0xFFFFFFFF);

  // Generate IPv6 address using EUI-64 format derived from mac address
  netif_ip6_addr_set(&netif, 0, ip_2_ip6(&ll_addr));
  netif_ip6_addr_set(&netif, 1, ip_2_ip6(&unicast_addr));
  netif_set_up(&netif);
  netif_ip6_addr_set_state(&netif, 0, IP6_ADDR_VALID);
  netif_ip6_addr_set_state(&netif, 1, IP6_ADDR_VALID);

  netif_set_link_up(&netif);

  /* add to relevant multicast groups */
  mld6_err = mld6_joingroup_netif(&netif, &multicast_glob_addr);
  if (mld6_err != ERR_OK) {
    printf("Could not join ff03::1\n");
  }

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

  bcmp_init(dfu_partition, device);
  bcmp_cli_init();

  bm_middleware_init(&netif, BM_MIDDLEWARE_PORT);

#ifdef STRESS_TEST_ENABLE
  stress_test_init(&netif, STRESS_TEST_PORT);
#endif
}

/*!
  Get string representation of IP address.
  NOTE: returns ptr to static buffer; not reentrant! (from ip6addr_ntoa)

  \param[in] ip IP address index (Since there are up to LWIP_IPV6_NUM_ADDRESSES)
  \return pointer to string with ip address
*/
const char *bcl_get_ip_str(uint8_t idx) {
  configASSERT(idx < LWIP_IPV6_NUM_ADDRESSES);
  return (ip6addr_ntoa(netif_ip6_addr(&netif, idx)));
}
