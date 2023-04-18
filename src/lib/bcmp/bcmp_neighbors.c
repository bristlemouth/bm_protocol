#include <string.h>
#include "FreeRTOS.h"
#include "bcmp.h"
#include "bcmp_neighbors.h"
#include "lwip/ip.h"

// Pointer to neighbor linked-list
static bm_neigbhor_t *_neighbors;

bm_neigbhor_t *bcmp_find_neighbor(const ip_addr_t *addr) {
  configASSERT(addr);
  bm_neigbhor_t *neighbor = _neighbors;

  while(neighbor != NULL) {
    if(ip6_addr_eq(addr, &neighbor->addr)) {
      // Found it!
      break;
    }

    // Go to the next one
    neighbor = neighbor->next;
  }

  return neighbor;
}

bm_neigbhor_t *bcmp_add_neighbor(const ip_addr_t *addr, uint8_t port) {
  bm_neigbhor_t *new_neighbor = pvPortMalloc(sizeof(bm_neigbhor_t));
  configASSERT(new_neighbor);

  memset(new_neighbor, 0, sizeof(bm_neigbhor_t));

  new_neighbor->addr = *addr;
  new_neighbor->port = port;

  bm_neigbhor_t *neighbor = NULL;
  if(_neighbors == NULL) {
    // First neighbor!
    _neighbors = new_neighbor;
  } else {
    neighbor = _neighbors;

    // Go to the last neighbor and insert the new one there
    while(neighbor && (neighbor->next != NULL)) {
      if(ip6_addr_eq(addr, &neighbor->addr)) {
        neighbor = NULL;
        break;
      }

      // Go to the next one
      neighbor = neighbor->next;
    }

    if(neighbor != NULL) {
      neighbor->next = new_neighbor;
    } else {
      vPortFree(new_neighbor);
    }
  }

  return new_neighbor;
}

bm_neigbhor_t * bcmp_update_neighbor(const ip_addr_t *addr, uint8_t port) {
  bm_neigbhor_t *neighbor = bcmp_find_neighbor(addr);

  if(neighbor == NULL) {
    printf("🏠🏠 Adding new neighbor! %s\n", ipaddr_ntoa(addr));
    neighbor = bcmp_add_neighbor(addr, port);

    // TODO - Send a request for neighbor info for the new neighbor
  }

  return neighbor;
}
