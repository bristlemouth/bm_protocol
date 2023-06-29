#pragma once

#include "FreeRTOS.h"
#include "task.h"
#include "configuration.h"
#include "bridgePowerController.h"

#include <stdint.h>

void topology_sampler_init(BridgePowerController *power_controller, cfg::Configuration* hw_cfg, cfg::Configuration* sys_cfg);
