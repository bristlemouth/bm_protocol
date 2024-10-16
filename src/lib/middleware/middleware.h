#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
#include "util.h"

#ifndef middleware_net_task_priority
#define middleware_net_task_priority 4
#endif

BmErr bm_middleware_local_pub(void *buf, uint32_t size);
BmErr bm_middleware_init(uint16_t port);
BmErr bm_middleware_net_tx(void *buf, uint32_t size);

#ifdef __cplusplus
}
#endif
