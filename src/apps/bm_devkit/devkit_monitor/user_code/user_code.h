#include "serial.h"
#include "bsp.h"
#include "debug_configuration.h"

#pragma once

// Declare extern sys partition configs we want to use from app main
// example CLI: cfg sys set sensorsPollIntervalMs uint 2000
//              cfg sys save
//        `     cfg sys listkeys
extern uint32_t sys_cfg_sensorsPollIntervalMs;

// app_main passes user space a handle to the user config partition, so we can define customer configs in our app.
extern cfg::Configuration* userConfigurationPartition;

// Simple Mutex for users to use, declared in app_main.
extern SemaphoreHandle_t xUserDataMutex;

void setup(void);
void loop(void);
