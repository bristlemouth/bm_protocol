#ifndef __BM_CONFIG_H__
#define __BM_CONFIG_H__

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "bm_l2.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef enum {
    BM_NETDEV_NONE,
    BM_NETDEV_ADIN,
    BM_NETDEV_MAX_DEVICES
} bm_netdev_type_t;

typedef struct bm_netdev_t {
    bm_netdev_type_t devices[2];
} bm_netdev_t;

extern bm_netdev_t bm_net_devices;

#ifdef __cplusplus
}
#endif

#endif /* __BM_CONFIG_H__ */