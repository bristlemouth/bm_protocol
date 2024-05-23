#include "mock_bm_network.h"

DEFINE_FAKE_VALUE_FUNC(bool, spotter_tx_data, const void *, uint16_t, bm_serial_network_type_e);
