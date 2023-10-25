#pragma once
#include "FreeRTOS.h"
#include "bridgePowerController.h"
#include "configuration.h"
#include "task.h"
#include "timers.h"
#include "topology_sampler.h"
#include <stdint.h>
#include <stdlib.h>

void aanderaControllerInit(BridgePowerController *power_controller,
                           cfg::Configuration *usr_cfg);
