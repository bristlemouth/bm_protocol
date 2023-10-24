#pragma once
#include <stdint.h>
#include <stdlib.h>
#include "FreeRTOS.h"
#include "task.h"
#include "topology_sampler.h"
#include "timers.h"
#include "bridgePowerController.h"

void aanderaControllerInit(BridgePowerController *power_controller);
