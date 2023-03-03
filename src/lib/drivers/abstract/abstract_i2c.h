//
// Created by Genton Mo on 1/7/21.
//

#pragma once

#include "FreeRTOS.h"
#include "protected_i2c.h"

#define I2C_INVALID_ADDRESS 0xF7  // typically a reserved I2C address for 10-bit addresses
#define I2C_DEFAULT_TIMEOUT_MS 10

class AbstractI2C {
public:
  AbstractI2C();

  // Virtual functions, derived class should not implement these functions
  virtual I2CResponse_t writeBytes(uint8_t * txBytes, size_t txLen, uint32_t timeout = I2C_DEFAULT_TIMEOUT_MS);
  virtual I2CResponse_t readBytes(uint8_t * rxBuff, size_t rxLen, uint32_t timeout = I2C_DEFAULT_TIMEOUT_MS);
  virtual I2CResponse_t probe();

  // Pure virtual functions, derived classes must implement these functions
  virtual bool init() = 0;

protected:
  I2CInterface_t * _interface;
  uint16_t _addr;
};
