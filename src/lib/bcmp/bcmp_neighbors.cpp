#include <string.h>

#include "bcmp_info.h"
#include "bcmp_neighbors.h"
#include "bm_util.h"

extern "C" {
#include "bm_os.h"
}

// Pointer to neighbor linked-list
static BcmpNeighbor *NEIGHBORS;
static uint8_t NUM_NEIGHBORS = 0;
NeighborDiscoveryCallback NEIGHBOR_DISCOVERY_CB = NULL;
/*!
  @brief Accessor to latest the neighbor linked-list and nieghbor count

  @param[out] &num_neighbors - number of neighbors
  @return - pointer to neighbors linked-list
*/
BcmpNeighbor *bcmp_get_neighbors(uint8_t &num_neighbors) {
  bcmp_check_neighbors();
  num_neighbors = NUM_NEIGHBORS;
  return NEIGHBORS;
}

/*!
  @bried Find neighbor entry in neighbor table

  @param node_id - neighbor's node_id
  @return pointer to neighbor if successful, NULL otherwise
*/
BcmpNeighbor *bcmp_find_neighbor(uint64_t node_id) {
  BcmpNeighbor *neighbor = NEIGHBORS;

  while (neighbor != NULL) {
    if (node_id && node_id == neighbor->node_id) {
      // Found it!
      break;
    }

    // Go to the next one
    neighbor = neighbor->next;
  }

  return neighbor;
}

/*!
  @brief Iterate through all neighbors and call callback function for each

  @param *callback - callback function to call for each neighbor
  @return none
*/
void bcmp_neighbor_foreach(NeighborCallback cb) {
  BcmpNeighbor *neighbor = NEIGHBORS;
  NUM_NEIGHBORS = 0;

  while (neighbor != NULL) {
    cb(neighbor);
    NUM_NEIGHBORS++;

    // Go to the next one
    neighbor = neighbor->next;
  }
}

/*!
  @brief Check If Neighbor Is Offline

  @details Neighbor will be online until a heartbeat is not seen for 2
           heartbeat periods or longer

  @param neighbor
 */
static void neighbor_check(BcmpNeighbor *neighbor) {
  if (neighbor->online &&
      !time_remaining(neighbor->last_heartbeat_ticks, bm_get_tick_count(),
                      bm_ms_to_ticks(2 * neighbor->heartbeat_period_s * 1000))) {
    printf("🏚  Neighbor offline :'( %016" PRIx64 "\n", neighbor->node_id);
    if (NEIGHBOR_DISCOVERY_CB) {
      NEIGHBOR_DISCOVERY_CB(false, neighbor);
    }
    neighbor->online = false;
  }
}

/*!
  Check neighbor livelyness status for all neighbors

  \return none
*/
void bcmp_check_neighbors() { bcmp_neighbor_foreach(neighbor_check); }

/*!
  @brief Add neighbor to neighbor table

  @param node_id - neighbor's node_id
  @param port - BM port mask
  @return pointer to neighbor if successful, NULL otherwise (if neighbor is already present, for example)
*/
static BcmpNeighbor *bcmp_add_neighbor(uint64_t node_id, uint8_t port) {
  BcmpNeighbor *new_neighbor = (BcmpNeighbor *)(bm_malloc(sizeof(BcmpNeighbor)));

  if (new_neighbor) {
    memset(new_neighbor, 0, sizeof(BcmpNeighbor));

    new_neighbor->node_id = node_id;
    new_neighbor->port = port;

    BcmpNeighbor *neighbor = NULL;
    if (NEIGHBORS == NULL) {
      // First neighbor!
      NEIGHBORS = new_neighbor;
    } else {
      neighbor = NEIGHBORS;

      // Go to the last neighbor and insert the new one there
      while (neighbor && (neighbor->next != NULL)) {
        if (node_id == neighbor->node_id) {
          neighbor = NULL;
          break;
        }

        // Go to the next one
        neighbor = neighbor->next;
      }

      if (neighbor != NULL) {
        neighbor->next = new_neighbor;
      } else {
        bm_free(new_neighbor);
        new_neighbor = NULL;
      }
    }
  }

  return new_neighbor;
}

/*!
  @brief Update neighbor information in neighbor table

  @param node_id - neighbor's node_id
  @param port - BM port mask for neighbor
  @return pointer to neighbor if successful, NULL otherwise
*/
BcmpNeighbor *bcmp_update_neighbor(uint64_t node_id, uint8_t port) {
  BcmpNeighbor *neighbor = bcmp_find_neighbor(node_id);

  if (neighbor == NULL) {
    printf("🏘  Adding new neighbor! %016" PRIx64 "\n", node_id);
    neighbor = bcmp_add_neighbor(node_id, port);
    // Let's get this node's information
    bcmp_request_info(node_id, &multicast_ll_addr);
  }

  return neighbor;
}

/*!
  @brief Free neighbor data from memory. NOTE: this does NOT remove neighbor from table

  @param *neighbor - neighbor to free
  @return true if the neighbor was freed, false otherwise
*/
bool bcmp_free_neighbor(BcmpNeighbor *neighbor) {
  bool rval = false;
  if (neighbor) {
    if (neighbor->version_str) {
      bm_free(neighbor->version_str);
    }

    if (neighbor->device_name) {
      bm_free(neighbor->device_name);
    }

    bm_free(neighbor);
    rval = true;
  }

  return rval;
}

/*!
  @brief Delete neighbor from neighbor table

  @param *neighbor - pointer to neighbor to remove
  @return true if successful, false otherwise
*/
bool bcmp_remove_neighbor_from_table(BcmpNeighbor *neighbor) {
  bool rval = false;

  if (neighbor) {
    // Check if we're the first in the table
    if (neighbor == NEIGHBORS) {
      printf("First neighbor!\n");
      // Remove neighbor from the list
      NEIGHBORS = neighbor->next;
      rval = true;
    } else {
      BcmpNeighbor *next_neighbor = NEIGHBORS;
      while (next_neighbor->next != NULL) {
        if (next_neighbor->next == neighbor) {

          // Found it!

          // Remove neighbor from the list
          next_neighbor->next = neighbor->next;
          rval = true;
          break;
        }

        // Go to the next one
        next_neighbor = neighbor->next;
      }
    }

    if (!rval) {
      printf("Something went wrong...\n");
    }

    // Free the neighbor
    rval = bcmp_free_neighbor(neighbor);
  }

  return rval;
}

/*!
  @brief Print neighbor information to CLI

  @param *neighbor - neighbor who's information we'll print
  @return true if successful, false otherwise
*/
void bcmp_print_neighbor_info(BcmpNeighbor *neighbor) {

  if (neighbor) {
    printf("Neighbor information:\n");
    printf("Node ID: %016" PRIx64 "\n", neighbor->node_id);
    printf("VID: %04X PID: %04X\n", neighbor->info.vendor_id, neighbor->info.product_id);
    printf("Serial number %.*s\n", 16, neighbor->info.serial_num);
    printf("GIT SHA: %" PRIX32 "\n", neighbor->info.git_sha);
    printf("Version: %u.%u.%u\n", neighbor->info.ver_major, neighbor->info.ver_minor,
           neighbor->info.ver_rev);
    printf("HW Version: %u\n", neighbor->info.ver_hw);
    if (neighbor->version_str) {
      printf("VersionStr: %s\n", neighbor->version_str);
    }
    if (neighbor->device_name) {
      printf("Device Name: %s\n", neighbor->device_name);
    }
  }
}

/*!
  @brief Register a callback to run whenever a neighbor is discovered / drops.

  @param *callback - callback function to run.
  @return none
*/
void bcmp_neighbor_register_discovery_callback(NeighborDiscoveryCallback cb) {
  NEIGHBOR_DISCOVERY_CB = cb;
}
