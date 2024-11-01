#pragma once
#include "FreeRTOS.h"
#include "abstractSensor.h"
#include "bridgePowerController.h"
#include "configuration.h"
#include "task.h"
#include "timers.h"
#include "topology_sampler.h"
#include <stdint.h>
#include <stdlib.h>

#define DEFAULT_TRANSMIT_AGGREGATIONS 1

typedef enum {
  SAMPLER_TIMER_BITS = 0x01,
  AGGREGATION_TIMER_BITS = 0x02,
} sensorControllerBits_t;

extern TaskHandle_t sensor_controller_task_handle;

void sensorControllerInit(BridgePowerController *power_controller);

AbstractSensor *sensorControllerFindSensorById(uint64_t node_id);
