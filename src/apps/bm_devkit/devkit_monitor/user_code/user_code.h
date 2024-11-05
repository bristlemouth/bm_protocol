#include "bsp.h"
#include "debug_configuration.h"
#include "serial.h"

#pragma once

// Declare extern sys partition configs we want to use from app main
// example CLI: cfg sys set sensorsPollIntervalMs uint 2000
//              cfg sys save
//        `     cfg sys listkeys
extern uint32_t sys_cfg_sensorsPollIntervalMs;

void setup(void);
void loop(void);
