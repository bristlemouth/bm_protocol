#pragma once

#include <stdbool.h>
#include <stdint.h>

extern "C" {
#include "device.h"
}
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

static constexpr uint32_t DEFAULT_MESSAGE_TIMEOUT_MS = 24;

BmErr bcmp_init(NvmPartition *dfu_partition, Configuration *sys_cfg, DeviceCfg device);
BmErr bcmp_tx(const void *dst, BcmpMessageType type, uint8_t *data, uint16_t size,
              uint32_t seq_num = 0, BmErr (*reply_cb)(uint8_t *payload) = NULL);
BmErr bcmp_ll_forward(BcmpHeader *header, void *data, uint32_t size, uint8_t ingress_port);
void bcmp_link_change(uint8_t port, bool state);
