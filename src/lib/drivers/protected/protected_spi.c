#include "protected_spi.h"
#include "log.h"

static Log_t *SPILog;

/*!
  Initialize an spi interface

  \param interface Handle to spi interface
  \return true if initialized successfully
*/
bool spiInit(SPIInterface_t *interface) {
  configASSERT(interface != NULL);

  if(SPILog == NULL) {
    SPILog = logCreate("SPI", "log", LOG_LEVEL_INFO, LOG_DEST_CONSOLE);
    logInit(SPILog);
  }

  bool rval = SPI_OK;

  interface->initFn();

  interface->mutex = xSemaphoreCreateMutex();
  configASSERT(interface->mutex != NULL);

  return rval;
}

void spiLoadLogCfg() {
  if(SPILog != NULL){
    logLoadCfg(SPILog, "lspi");

    if((SPILog->level == LOG_LEVEL_DEBUG) &&
        (SPILog->destination & LOG_DEST_FILE)) {
      // If we're logging SPI by default, use a huge buffer
      // so we don't blast the SD card
      logSetBuffSize(SPILog, 4096);
    }

    if((SPILog->destination & LOG_DEST_FILE) && (SPILog->buff == NULL)) {
      logSDSetup(SPILog);
    }
  }
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

    logPrint(SPILog, LOG_LEVEL_DEBUG, "%s [%s] ", __func__, interface->name);
    if(txBuff != NULL) {
      logPrintf(SPILog, LOG_LEVEL_DEBUG, "TX(%u) ", len);
      for(uint16_t idx = 0; idx < len; idx++) {
        logPrintf(SPILog, LOG_LEVEL_DEBUG, "%02X ", txBuff[idx]);
      }
    }

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

    if(rxBuff != NULL) {
      logPrintf(SPILog, LOG_LEVEL_DEBUG, "RX(%u) ", len);
      for(uint16_t idx = 0; idx < len; idx++) {
        logPrintf(SPILog, LOG_LEVEL_DEBUG, "%02X ", rxBuff[idx]);
      }
    }

    logPrintf(SPILog, LOG_LEVEL_DEBUG, "\n");

    xSemaphoreGive(interface->mutex);
  } else {
    logPrint(SPILog, LOG_LEVEL_WARNING, "%s Error [%s] - Unable to take mutex.\n", __func__, interface->name);
    rval = SPI_MUTEX;
  }

  return rval;
}
