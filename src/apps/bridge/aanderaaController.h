#pragma once
#include "FreeRTOS.h"
#include "bridgePowerController.h"
#include "configuration.h"
#include "task.h"
#include "timers.h"
#include "topology_sampler.h"
#include <stdint.h>
#include <stdlib.h>

#define DEFAULT_TRANSMIT_AGGREGATIONS 1
#define DEFAULT_SAMPLES_PER_REPORT 2

void aanderaControllerInit(BridgePowerController *power_controller,
                           cfg::Configuration *usr_cfg,
                           cfg::Configuration *sys_cfg);
