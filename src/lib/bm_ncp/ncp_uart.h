#pragma once

#include <stdbool.h>
#include <stdint.h>

// TODO - Will we eventually need this - probably for ota updates etc..
// #include "bm_dfu_message_structs.h"
#include "bsp.h"
#include "serial.h"
#include "nvmPartition.h"
#include "bridgePowerController.h"
#include "configuration.h"

#define NCP_BUFF_LEN 2048

// unused for now, but will be needed for bridge power sampling/interrupt driven uart comms
typedef struct {
  IOPinHandle_t *intPin;
  IOPinHandle_t *resetPin;
  // TODO - Define other bridge configs?
} NCPConfig_t;

void ncpInit(SerialHandle_t *ncpUartHandle, NvmPartition *dfu_partition, BridgePowerController *power_controller,
  cfg::Configuration* usr_cfg, cfg::Configuration* sys_cfg, cfg::Configuration* hw_cfg);

bool ncp_info_requested();
// bool bridgeStart(const BridgeConfig_t *config); // TODO - do we need something like this - probably?
