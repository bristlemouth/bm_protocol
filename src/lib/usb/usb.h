#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "serial.h"
#include "io.h"

#ifdef __cplusplus
extern "C" {
#endif

bool usbInit(IOPinHandle_t *vusbDetectPin, bool (*usbIsConnectedFn)());
void usbMspInit();
SerialHandle_t * serialHandleFromItf(uint8_t itf);
void usb_line_state_change(uint8_t itf, uint8_t dtr, bool rts);

#ifdef __cplusplus
}
#endif
