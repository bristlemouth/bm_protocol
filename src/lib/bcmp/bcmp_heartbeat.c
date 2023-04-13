#include "bcmp.h"
#include "FreeRTOS.h"
#include "task.h"
#include "bcmp_neighbors.h"

err_t bcmp_send_heartbeat(uint32_t period_s) {
  bcmpHeartbeat_t heartbeat = {
    .uptime_us = (uint64_t)xTaskGetTickCount() * 1000,
    .period_us = (uint64_t)period_s * 1000000};
  return bcmp_tx(&multicast_ll_addr, BCMP_HEARTBEAT, (uint8_t *)&heartbeat, sizeof(heartbeat));
}

err_t bcmp_process_heartbeat(void *payload, const ip_addr_t *src, uint8_t dst_port) {
  bcmpHeartbeat_t *heartbeat = (bcmpHeartbeat_t *)payload;

  // Print node's uptime in seconds.milliseconds
  uint64_t uptime_s = heartbeat->uptime_us/1e6;
  uint32_t uptime_ms = (uint32_t)(heartbeat->uptime_us/1000 - (uptime_s * 1000));
  printf("❤️  from %s (%"PRIu64".%03"PRIu32") [%02X]\n", ipaddr_ntoa(src), uptime_s, uptime_ms, dst_port);

  // TODO - add to neighbor table
  bcmp_update_neighbor(src, dst_port);

  return ERR_OK;
}
