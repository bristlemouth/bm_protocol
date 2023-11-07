#pragma once

#include "FreeRTOS.h"
#include "task.h"
#include "configuration.h"
#include "bridgePowerController.h"
#include "bm_common_structs.h"

#include <stdint.h>

#define TOPOLOGY_SAMPLER_MAX_NODE_LIST_SIZE (16) // Note: Derived from satellite message definition, 1 bridge + 15 more nodes.

void topology_sampler_init(BridgePowerController *power_controller, cfg::Configuration* hw_cfg, cfg::Configuration* sys_cfg);
bool topology_sampler_get_node_list(uint64_t *node_list, size_t &node_list_size, uint32_t &num_nodes, uint32_t timeout_ms);
uint8_t* topology_sampler_alloc_last_network_config(uint32_t &network_crc32, uint32_t &cbor_config_size);
bool bm_topology_last_network_info_cb(bm_common_network_info_t *network_info);