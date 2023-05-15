#include "mock_bm_serial.h"

DEFINE_FAKE_VALUE_FUNC(bm_serial_error_e,bm_serial_pub, uint64_t , const char *, uint16_t , const uint8_t *, uint16_t);
