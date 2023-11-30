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
  double abs_tilt_mean_rad;
  double std_tilt_mean_rad;
  uint32_t reading_count;
} aanderaa_aggregations_t;

typedef enum {
  SAMPLER_TIMER_BITS = 0x01,
  AGGREGATION_TIMER_BITS = 0x02,
} aanderaaControllerBits_t;

extern TaskHandle_t aanderaa_controller_task_handle;

#define AANDERAA_NUM_SAMPLE_MEMBERS 8

void aanderaControllerInit(BridgePowerController *power_controller,
                           cfg::Configuration *sys_cfg);
