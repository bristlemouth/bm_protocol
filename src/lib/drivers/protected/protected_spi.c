#include <stdio.h>
#include "protected_spi.h"

/*!
  Initialize an spi interface

  \param interface Handle to spi interface
  \return true if initialized successfully
*/
bool spiInit(SPIInterface_t *interface) {
  configASSERT(interface != NULL);

  bool rval = SPI_OK;

  interface->initFn();

  interface->mutex = xSemaphoreCreateMutex();
  configASSERT(interface->mutex != NULL);

  return rval;
}

/*!
   Write and/or read data from spi device

  \param interface Handle to spi interface
  \param len spi transaction length
  \param txBuff tx buffer, set to NULL if unused
  \param rxBuff rx buffer, set to NULL if unused
  \param timeoutMs timeout before giving up
  \return SPIResponse depending on how it goes
*/
SPIResponse_t spiTxRx(SPIInterface_t *interface, IOPinHandle_t *csPin, size_t len, uint8_t *txBuff, uint8_t *rxBuff, uint32_t timeoutMs) {
  configASSERT(interface != NULL);
  SPIResponse_t rval = SPI_ERR;

  if(xSemaphoreTake(interface->mutex, pdMS_TO_TICKS(timeoutMs)) == pdTRUE) {

#ifdef SPI_DEBUG
    printf("%s [%s] ", __func__, interface->name);
    if(txBuff != NULL) {
      printf("TX(%u) ", len);
      for(uint16_t idx = 0; idx < len; idx++) {
        printf("%02X ", txBuff[idx]);
      }
    }
#endif

    HAL_StatusTypeDef halRval = HAL_ERROR;

    // Assert CS if needed
    if(csPin != NULL) {
      IOWrite(csPin, 0);
    }

    if((txBuff != NULL) && (rxBuff != NULL)) {
      halRval = HAL_SPI_TransmitReceive(interface->handle, txBuff, rxBuff, len, timeoutMs);
    } else if (txBuff != NULL) {
      halRval = HAL_SPI_Transmit(interface->handle, txBuff, len, timeoutMs);
    } else if (rxBuff != NULL) {
      halRval = HAL_SPI_Receive(interface->handle, rxBuff, len, timeoutMs);
    }

    // De-assert CS if needed
    if(csPin != NULL) {
      IOWrite(csPin, 1);
    }

    switch(halRval) {
      case HAL_OK: {
        rval = SPI_OK;
        break;
      }
      case HAL_ERROR: {
        rval = SPI_ERR;
        break;
      }
      case HAL_BUSY: {
        rval = SPI_ERR;
        break;
      }
      case HAL_TIMEOUT: {
        rval = SPI_TIMEOUT;
        break;
      }
    }

#ifdef SPI_DEBUG
    if(rxBuff != NULL) {
      printf("RX(%u) ", len);
      for(uint16_t idx = 0; idx < len; idx++) {
        printf("%02X ", rxBuff[idx]);
      }
    }

    logPrintf(SPILog, LOG_LEVEL_DEBUG, "\n");
#endif

    xSemaphoreGive(interface->mutex);
  } else {
    printf("%s Error [%s] - Unable to take mutex.\n", __func__, interface->name);
    rval = SPI_MUTEX;
  }

  return rval;
}
