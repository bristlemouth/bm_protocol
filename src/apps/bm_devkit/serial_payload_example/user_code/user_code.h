#include "serial.h"
#include "bsp.h"
#include "debug_configuration.h"

#pragma once

void setup(void);
void loop(void);

typedef enum
{
  MODE_RS232 = 0,  // Default mode
  MODE_RS485_HD,  // Sets the RS-485 transceiver to Half-Duplex mode,
                  // and sets PLUART transactions to switch between Tx and Rx appropriately.
  MODE_SDI12,     // Enables the SDI12 transceiver, sets PLUART to SDI12 settings,
                  // and sets PLUART transactions to switch between Tx and Rx appropriately.
  MODE_MAX
} PLUARTMode;
