#ifndef __BM_CONFIG_H__
#define __BM_CONFIG_H__

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "adin2111.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define ADIN_PORT_MASK_1     (1 << ADIN2111_PORT_1)
#define ADIN_PORT_MASK_2     (1 << ADIN2111_PORT_2)
#define ADIN_PORT_MASK_ALL   (ADIN_PORT_MASK_1 | ADIN_PORT_MASK_2)

typedef struct adin2111_config_s {
    uint32_t port_mask;
    adin2111_DeviceHandle_t dev;
} adin2111_config_t;

typedef enum {
    BM_NETDEV_TYPE_NONE,
    BM_NETDEV_TYPE_ADIN2111,
    BM_NETDEV_TYPE_MAX
} bm_netdev_type_t;

typedef struct bm_netdev_config_s {
    bm_netdev_type_t type;
    void * config;
} bm_netdev_config_t;

/* Define for actual netdev instance count here, as some of the later code currently iterates devices
   using BM_NETDEV_MAX_DEVICES (which specifies the number of enum entries, not the actual number of devices).
   If you added a new enum entry, but didn't change the instance array, the current code would crash. */
#define BM_NETDEV_COUNT (2)

extern const bm_netdev_config_t bm_netdev_config[BM_NETDEV_COUNT];

#ifdef __cplusplus
}
#endif

#endif /* __BM_CONFIG_H__ */
