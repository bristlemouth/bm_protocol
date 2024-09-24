#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "lwip/ip_addr.h"
#include "lwip/netif.h"

#include "bcmp_messages.h"
#include "bm_util.h"
#include "messages.h"
#include "util.h"

#include "configuration.h"
#include "nvmPartition.h"

using namespace cfg;
/* Ingress and Egress ports are mapped to the 5th and 6th byte of the IPv6 src address as per
    the bristlemouth protocol spec */
#define CLEAR_PORTS(x) (x[1] &= (~(0xFFFFU)))

#define IP_PROTO_BCMP (0xBC)
#define BCMP_MAX_PAYLOAD_SIZE_BYTES (1500) // FIXME: Remove when we can split payloads.

/**
 * @brief A function pointer type for handling reply messages in the BCMP protocol.
 *
 * This typedef defines a type for function pointers that can be used as callbacks
 * to handle sequenced reply messages in the BCMP protocol. These functions should
 * take a pointer to a payload of type uint8_t as a parameter and return a boolean
 * value.
 *
 * The return value should be:
 * - true if the reply message was handled successfully,
 * - false if there was an error handling the reply message.
 */
typedef bool (*bcmp_reply_message_cb)(uint8_t *payload);

static constexpr uint32_t DEFAULT_MESSAGE_TIMEOUT_MS = 24;

void bcmp_init(struct netif *netif, NvmPartition *dfu_partition, Configuration *user_cfg,
               Configuration *sys_cfg);
BmErr bcmp_tx(const ip_addr_t *dst, BcmpMessageType type, uint8_t *buff, uint16_t len,
              uint16_t seq_num = 0, BmErr (*reply_cb)(uint8_t *payload) = NULL,
              uint32_t request_timeout_ms = DEFAULT_MESSAGE_TIMEOUT_MS);
BmErr bcmp_ll_forward(BcmpHeader *header, void *data, uint32_t size, uint8_t ingress_port);
void bcmp_link_change(uint8_t port, bool state);
