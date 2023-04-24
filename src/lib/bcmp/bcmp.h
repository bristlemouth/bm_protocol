#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "lwip/netif.h"
#include "lwip/ip_addr.h"

#include "bcmp_messages.h"
#include "bm_util.h"

#include "nvmPartition.h"

/* Ingress and Egress ports are mapped to the 5th and 6th byte of the IPv6 src address as per
    the bristlemouth protocol spec */
#define CLEAR_PORTS(x) (x[1] &= (~(0xFFFFU)))

#define IP_PROTO_BCMP (0xBC)

void bcmp_init(struct netif* netif, NvmPartition * dfu_partition);
err_t bcmp_tx(const ip_addr_t *dst, bcmp_message_type_t type, uint8_t *buff, uint16_t len);
void bcmp_link_change(uint8_t port, bool state);
