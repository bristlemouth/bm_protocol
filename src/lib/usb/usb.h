#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "serial.h"

#ifdef __cplusplus
extern "C" {
#endif

bool usbInit();
void usbMspInit();
SerialHandle_t * serialHandleFromItf(uint8_t itf);
void usb_line_state_change(uint8_t itf, uint8_t dtr, bool rts);

#ifdef __cplusplus
}
#endif
