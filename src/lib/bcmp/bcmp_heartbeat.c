#include "bcmp.h"
#include "FreeRTOS.h"
#include "task.h"
#include "bcmp_neighbors.h"

err_t bcmp_send_heartbeat(uint32_t period_s) {
  bcmp_heartbeat_t heartbeat = {
    .time_since_boot_us = (uint64_t)xTaskGetTickCount() * 1000,
    .liveliness_lease_dur_s = (uint64_t)period_s * 1000000};
  return bcmp_tx(&multicast_ll_addr, BCMP_HEARTBEAT, (uint8_t *)&heartbeat, sizeof(heartbeat));
}

err_t bcmp_process_heartbeat(void *payload, const ip_addr_t *src, uint8_t dst_port) {
  bcmp_heartbeat_t *heartbeat = (bcmp_heartbeat_t *)payload;

  // Print node's uptime in seconds.milliseconds
  uint64_t uptime_s = heartbeat->time_since_boot_us/1e6;
  uint32_t uptime_ms = (uint32_t)(heartbeat->time_since_boot_us/1000 - (uptime_s * 1000));
  printf("❤️  from %s (%"PRIu64".%03"PRIu32") [%02X]\n", ipaddr_ntoa(src), uptime_s, uptime_ms, dst_port);

  // TODO - add to neighbor table
  bcmp_update_neighbor(src, dst_port);

  return ERR_OK;
}
