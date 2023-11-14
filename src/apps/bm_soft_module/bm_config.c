#include "bm_config.h"

static adin2111_DeviceStruct_t adin_device;

static adin2111_config_t adin_cfg = {
    .port_mask = ADIN_PORT_MASK_ALL,
    .dev = &adin_device,
};

const bm_netdev_config_t bm_netdev_config[] = {{BM_NETDEV_TYPE_ADIN2111, &adin_cfg}, {BM_NETDEV_TYPE_NONE, NULL}};
