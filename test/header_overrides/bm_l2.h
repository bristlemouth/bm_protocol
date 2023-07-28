#pragma once
#include <stdint.h>
#include "bm_config.h"
#include "fff.h"

DECLARE_FAKE_VALUE_FUNC(uint8_t, bm_l2_get_num_ports);
DECLARE_FAKE_VOID_FUNC(bm_l2_netif_set_power, void *, bool);
DECLARE_FAKE_VALUE_FUNC(bool, bm_l2_get_device_handle, uint8_t, void **, bm_netdev_type_t *, uint32_t *);
