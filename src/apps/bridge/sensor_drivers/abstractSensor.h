#pragma once
#include "FreeRTOS.h"
#include "semphr.h"
#include <stdint.h>

typedef enum abstractSensorType {
    SENSOR_TYPE_UNKNOWN = 0,
    SENSOR_TYPE_AANDERAA = 1,
} abstractSensorType_e;

struct AbstractSensor {
    uint64_t node_id;
    AbstractSensor *next;
    SemaphoreHandle_t _mutex;
    abstractSensorType_e type;
public:
    virtual bool subscribe() = 0;
};
