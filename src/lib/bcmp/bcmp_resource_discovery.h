#pragma once
#include "bcmp_messages.h"
#include <stdint.h>
#include "lwip/ip_addr.h"

namespace bcmp_resource_discovery {

#define DEFAULT_RESOURCE_ADD_TIMEOUT_MS (100)

void bcmp_process_resource_discovery_request(bcmp_resource_table_request_t *req, const ip_addr_t *dst);
void bcmp_process_resource_discovery_reply(bcmp_resource_table_reply_t *repl,  uint64_t source_id);
void bcmp_resource_discovery_init(void);
bool bcmp_resource_discovery_add_resource(const char * res, const uint16_t resource_len, uint32_t timeoutMs=DEFAULT_RESOURCE_ADD_TIMEOUT_MS);
bool bcmp_resource_discovery_get_num_resources(uint16_t& num_resources, uint32_t timeoutMs);
bool bcmp_resource_discovery_find_resource(const char * res, const uint16_t resource_len, bool &found, uint32_t timeoutMs);
bool bcmp_resource_discovery_send_request(uint64_t target_node_id);
void bcmp_resource_discovery_print_resources(void);

}

