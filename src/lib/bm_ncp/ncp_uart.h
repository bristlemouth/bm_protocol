#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "ncp_messages.h"
// TODO - Will we eventually need this - probably for ota updates etc..
// #include "bm_dfu_message_structs.h"
#include "bsp.h"
#include "serial.h"

#define NCP_BUFF_LEN 2048

// unused for now, but will be needed for bridge power sampling/interrupt driven uart comms
typedef struct {
  IOPinHandle_t *intPin;
  IOPinHandle_t *resetPin;
  // TODO - Define other bridge configs?
} NCPConfig_t;

void ncpInit(SerialHandle_t *ncpUartHandle);
// bool bridgeStart(const BridgeConfig_t *config); // TODO - do we need something like this - probably?

bool ncpTx(bcmp_ncp_message_t type, uint8_t *buff, size_t len);
BaseType_t ncpRXBytesFromISR(SerialHandle_t *handle, uint8_t *buffer, size_t len);
