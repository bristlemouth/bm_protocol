#include "bcmp_heartbeat.h"
#include "bcmp.h"
#include "bcmp_info.h"
#include "bcmp_neighbors.h"
#include "bm_util.h"
extern "C" {
#include "bm_os.h"
#include "packet.h"
}

extern NeighborDiscoveryCallback
    NEIGHBOR_DISCOVERY_CB; // FIXME - https://github.com/wavespotter/bristlemouth/issues/384

/*!
  Send heartbeat to neighbors

  \param lease_duration_s - liveliness lease duration
  \return ERR_OK if successful
*/
BmErr bcmp_send_heartbeat(uint32_t lease_duration_s) {
  // TODO: abstract time since boot microseconds?
  BcmpHeartbeat heartbeat = {.time_since_boot_us = bm_ticks_to_ms(bm_get_tick_count()) * 1000,
                             .liveliness_lease_dur_s = lease_duration_s};
  return bcmp_tx(&multicast_ll_addr, BcmpHeartbeatMessage, (uint8_t *)&heartbeat,
                 sizeof(heartbeat));
}

/*!
  Process incoming heartbeat. Update neighbor tables if necessary.
  If a device is not in our neighbor tables, add it, and request it's info.

  \param *heartbeat - hearteat data
  \return ERR_OK if successful
*/
static BmErr bcmp_process_heartbeat(BcmpProcessData data) {
  BmErr err = BmEINVAL;
  BcmpHeartbeat *heartbeat = (BcmpHeartbeat *)data.payload;

  BcmpNeighbor *neighbor = bcmp_update_neighbor(ip_to_nodeid(data.src), data.ingress_port);
  if (neighbor) {
    err = BmOK;

    // Neighbor restarted, let's get additional info
    if (heartbeat->time_since_boot_us < neighbor->last_time_since_boot_us) {
      if (NEIGHBOR_DISCOVERY_CB) {
        NEIGHBOR_DISCOVERY_CB(true, neighbor);
      }
      printf("🏘📡 Updating neighbor info! %016" PRIx64 "\n", neighbor->node_id);
      bcmp_request_info(neighbor->info.node_id, &multicast_ll_addr);
    }

    // Update times
    neighbor->last_time_since_boot_us = heartbeat->time_since_boot_us;
    neighbor->heartbeat_period_s = heartbeat->liveliness_lease_dur_s;
    neighbor->last_heartbeat_ticks = bm_get_tick_count();
    neighbor->online = true;
  }

  return err;
}

BmErr heartbeat_init(void) {
  BcmpPacketCfg heartbeat_packet = {
      false,
      false,
      bcmp_process_heartbeat,
  };

  return packet_add(&heartbeat_packet, BcmpHeartbeatMessage);
}
