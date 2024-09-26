#pragma once

#include "FreeRTOS.h"
#include "lwip/ip_addr.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
#include "util.h"

extern const ip6_addr_t multicast_global_addr;
extern const ip6_addr_t multicast_ll_addr;

//TODO: make this endian agnostic and platform agnostic
/*!
  Extract 64-bit node id from IP address

  \param *ip address to extract node id from
  \return node id
*/
static inline uint64_t ip_to_nodeid(void *ip) {
  uint32_t msb = ((uint32_t *)(ip))[2];
  uint32_t lsb = ((uint32_t *)(ip))[3];
  swap_32bit(&msb);
  swap_32bit(&lsb);
  return (uint64_t)msb << 32 | (uint64_t)lsb;
}

#ifdef __cplusplus
}
#endif
