#include "bm_l2.h"

DEFINE_FAKE_VALUE_FUNC(uint8_t,bm_l2_get_num_ports);
DEFINE_FAKE_VOID_FUNC(bm_l2_netif_set_power, void *, bool);
DEFINE_FAKE_VALUE_FUNC(bool, bm_l2_get_device_handle, uint8_t, void **, bm_netdev_type_t *, uint32_t *);
