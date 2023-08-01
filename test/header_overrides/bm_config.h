#ifndef __BM_CONFIG_H__
#define __BM_CONFIG_H__

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

// https://github.com/wavespotter/bristlemouth/issues/423 - Move this file into apps

typedef enum {
    BM_NETDEV_TYPE_NONE,
    BM_NETDEV_TYPE_ADIN2111,
    BM_NETDEV_TYPE_MAX
} bm_netdev_type_t;

typedef struct bm_netdev_config_s {
    bm_netdev_type_t type;
    // Additional config can be added here later
} bm_netdev_config_t;

/* Define for actual netdev instance count here, as some of the later code currently iterates devices
   using BM_NETDEV_MAX_DEVICES (which specifies the number of enum entries, not the actual number of devices).
   If you added a new enum entry, but didn't change the instance array, the current code would crash. */
#define BM_NETDEV_COUNT (2)

extern bm_netdev_config_t bm_netdev_config[BM_NETDEV_COUNT];

#ifdef __cplusplus
}
#endif

#endif /* __BM_CONFIG_H__ */
