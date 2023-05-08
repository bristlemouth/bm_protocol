#pragma once

#include <stdint.h>
#include "bridgePowerController.h"
#include "FreeRTOS_CLI.h"

#ifdef __cplusplus
extern "C" {
#endif

void debugBridgePowerControllerInit(BridgePowerController *bridge_power_controller);

#ifdef __cplusplus
}
#endif
