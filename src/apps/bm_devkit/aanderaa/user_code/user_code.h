#include "serial.h"
#include "bsp.h"

#pragma once

// Simple Mutex for users to use, declared in app_main
extern SemaphoreHandle_t xUserDataMutex;

void setup(void);
void loop(void);
