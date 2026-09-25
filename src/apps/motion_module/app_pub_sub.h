#pragma once

#include <stdint.h>
#include "bm_common_pub_sub.h"

#ifdef __cplusplus
extern "C" {
#endif

#define APP_PUB_SUB_UTC_TOPIC           "spotter/utc-time"
#define APP_PUB_SUB_UTC_TYPE            1
#define APP_PUB_SUB_UTC_VERSION         1

// A target_node_id of 0 means all targets
#define BM_ALL_NODES                    0

#ifdef __cplusplus
}
#endif
