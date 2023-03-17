#pragma once

#include "FreeRTOS.h"
#include "protected_spi.h"

#define SPI_DEFAULT_TIMEOUT_MS 10

class AbstractSPI {
public:
  AbstractSPI(SPIInterface_t *interface, IOPinHandle_t *csPin);

  // Virtual functions, derived class should not implement these functions
  virtual SPIResponse_t writeBytes(uint8_t * txBytes, size_t txLen, uint32_t timeout = SPI_DEFAULT_TIMEOUT_MS);
  virtual SPIResponse_t readBytes(uint8_t * rxBuff, size_t rxLen, uint32_t timeout = SPI_DEFAULT_TIMEOUT_MS);
  virtual SPIResponse_t writeReadBytes(uint8_t * rxBuff, size_t len, uint8_t * txBytes, uint32_t timeout = SPI_DEFAULT_TIMEOUT_MS);

protected:
  SPIInterface_t * _interface;
  IOPinHandle_t *_csPin;
};
