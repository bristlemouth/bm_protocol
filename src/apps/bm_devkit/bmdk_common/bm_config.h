#ifndef __BM_CONFIG_H__
#define __BM_CONFIG_H__

#include "adin2111.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define ADIN_PORT_MASK_1 (1 << ADIN2111_PORT_1)
#define ADIN_PORT_MASK_2 (1 << ADIN2111_PORT_2)
#define ADIN_PORT_MASK_ALL (ADIN_PORT_MASK_1 | ADIN_PORT_MASK_2)

typedef struct adin2111_config_s {
  uint32_t port_mask;
  adin2111_DeviceHandle_t dev;
} adin2111_config_t;

#ifdef __cplusplus
}
#endif

#endif /* __BM_CONFIG_H__ */
