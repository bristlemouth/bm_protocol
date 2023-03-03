//
// Created by Genton Mo on 1/7/21.
//

#include "abstract_i2c.h"

AbstractI2C::AbstractI2C():
  _interface(NULL),
  _addr(I2C_INVALID_ADDRESS)
{}

I2CResponse_t AbstractI2C::writeBytes(uint8_t *txBytes, size_t txLen, uint32_t timeout) {
  I2CResponse_t rval = I2C_OK;
  if (txLen && txBytes != nullptr) {
    rval = i2cTx(_interface, _addr, txBytes, txLen, timeout);
  }
  return rval;
}

I2CResponse_t AbstractI2C::readBytes(uint8_t *rxBuff, size_t rxLen, uint32_t timeout) {
  I2CResponse_t rval = I2C_OK;
  if (rxLen && (rxBuff != nullptr)) {
    rval = i2cRx(_interface, _addr, rxBuff, rxLen, timeout);
  }
  return rval;
}

I2CResponse_t AbstractI2C::probe() {
  I2CResponse_t rval = I2C_OK;

  rval = i2cProbe(_interface, _addr, 55);

  return rval;
}
