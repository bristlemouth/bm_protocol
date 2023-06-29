#include "bcmp.h"
#include "FreeRTOS.h"
#include "task.h"
#include "bcmp_neighbors.h"
#include "bcmp_heartbeat.h"
#include "bcmp_info.h"
#include "device_info.h"
#include "uptime.h"

extern neighbor_discovery_callback_t neighbor_discovery_cb; // FIXME - https://github.com/wavespotter/bristlemouth/issues/384

/*!
  Send heartbeat to neighbors

  \param lease_duration_s - liveliness lease duration
  \return ERR_OK if successful
*/
err_t bcmp_send_heartbeat(uint32_t lease_duration_s) {
  bcmp_heartbeat_t heartbeat = {
    .time_since_boot_us = uptimeGetMicroSeconds(),
    .liveliness_lease_dur_s = lease_duration_s};
  return bcmp_tx(&multicast_ll_addr, BCMP_HEARTBEAT, (uint8_t *)&heartbeat, sizeof(heartbeat));
}

/*!
  Process incoming heartbeat. Update neighbor tables if necessary.
  If a device is not in our neighbor tables, add it, and request it's info.

  \param *heartbeat - hearteat data
  \return ERR_OK if successful
*/
err_t bcmp_process_heartbeat(bcmp_heartbeat_t *heartbeat, const ip_addr_t *src, uint8_t dst_port) {
  configASSERT(heartbeat);

  // Print node's uptime in seconds.milliseconds
  // uint64_t uptime_s = heartbeat->time_since_boot_us/1e6;
  // uint32_t uptime_ms = (uint32_t)(heartbeat->time_since_boot_us/1000 - (uptime_s * 1000));
  // printf("❤️  from %"PRIx64" (%"PRIu64".%03"PRIu32") [%02X]\n", ip_to_nodeid(src), uptime_s, uptime_ms, dst_port);

  bm_neighbor_t *neighbor = bcmp_update_neighbor(ip_to_nodeid(src), dst_port);
  if(neighbor) {

    // Neighbor restarted, let's get additional info
    if(heartbeat->time_since_boot_us < neighbor->last_time_since_boot_us) {
      if(neighbor_discovery_cb) {
        neighbor_discovery_cb(true,neighbor);
      }
      printf("🏘📡 Updating neighbor info! %" PRIx64 "\n", neighbor->node_id);
      bcmp_request_info(neighbor->info.node_id, &multicast_ll_addr);
    }

    // Update times
    neighbor->last_time_since_boot_us = heartbeat->time_since_boot_us;
    neighbor->heartbeat_period_s = heartbeat->liveliness_lease_dur_s;
    neighbor->last_heartbeat_ticks = xTaskGetTickCount();
    neighbor->online = true;
  }

  return ERR_OK;
}
