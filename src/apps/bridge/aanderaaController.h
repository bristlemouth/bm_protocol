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

typedef struct aanderaa_aggregations_s {
  double abs_speed_mean_cm_s;
  double abs_speed_std_cm_s;
  double direction_circ_mean_rad;
  double direction_circ_std_rad;
  double temp_mean_deg_c;
} aanderaa_aggregations_t;

void aanderaControllerInit(BridgePowerController *power_controller,
                           cfg::Configuration *usr_cfg,
                           cfg::Configuration *sys_cfg);
