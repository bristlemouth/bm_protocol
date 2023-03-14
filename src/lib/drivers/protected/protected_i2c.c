#include <stdio.h>
#include "protected_i2c.h"

// Translate HAL i2c error codes to ours
static I2CResponse_t _halI2cErrToI2CResponse(uint32_t errorCode) {
  I2CResponse_t rval = I2C_ERR; // Generic error

  if(errorCode & HAL_I2C_ERROR_AF) {
    rval = I2C_NACK;
  } else if(errorCode & HAL_I2C_ERROR_TIMEOUT) {
    rval = I2C_TIMEOUT;
  } else if(errorCode == 0) {
    rval = I2C_OK;
  }

  return rval;
}

// Sometimes the i2c bus gets wedged in a state where all transactions fail
// While we figure out the root cause, re-initializing the interface
// seems to clear the problem
#if I2C_WORKAROUND == 1
static void i2cWorkaround(I2CInterface_t *interface, I2CResponse_t rval) {
  if(rval == I2C_TIMEOUT || rval == I2C_ERR) {
    printf("(Workaround) Re-initializing interface [%s]\n", interface->name);
    interface->initFn();
  }
}
#endif

/*!
  i2cInit(I2CInterface_t *interface)
  \brief Initialize an i2c interface
  \param interface Handle to i2c interface
  \return I2C_OK if device present
*/
bool i2cInit(I2CInterface_t *interface) {
  configASSERT(interface != NULL);

  I2CResponse_t rval = true;

  interface->initFn();

  interface->mutex = xSemaphoreCreateMutex();
  configASSERT(interface->mutex != NULL);

  return rval;
}

/*!
  I2CResponse_t i2cTxRx(I2CInterface_t *interface, uint8_t address, uint8_t *txBuff, size_t txLen, uint8_t *rxBuff, size_t rxLen, uint32_t timeoutMs)
  \brief Write and/or read data from i2c device
  \param interface Handle to i2c interface
  \param address i2c device address
  \param txBuff tx buffer, set to NULL if unused
  \param txLen number of bytes to transmit
  \param rxBuff rx buffer, set to NULL if unused
  \param rxLen number of bytes to receive
  \param timeoutMs timeout before giving up
  \return I2CResponse depending on how it goes
*/
I2CResponse_t i2cTxRx(I2CInterface_t *interface, uint8_t address, uint8_t *txBuff, size_t txLen, uint8_t *rxBuff, size_t rxLen, uint32_t timeoutMs) {
  configASSERT(interface != NULL);
  I2CResponse_t rval = I2C_ERR;

  // Make sure interface has been initialized!
  configASSERT(interface->mutex != NULL);

  if(xSemaphoreTake(interface->mutex, pdMS_TO_TICKS(timeoutMs)) == pdTRUE) {

#ifdef I2C_DEBUG
    printf("%s [%s] ", __func__, interface->name);
    if(txLen) {
      printf("TX(%u) ", txLen);
      for(uint16_t idx = 0; idx < txLen; idx++) {
        printf("%02X ", txBuff[idx]);
      }
    }
#endif

    HAL_StatusTypeDef hal_rval;
    do {
      if(txBuff != NULL && txLen > 0) {
        hal_rval = HAL_I2C_Master_Transmit(interface->handle, address << 1, txBuff, txLen, timeoutMs);
        if(hal_rval != HAL_OK) {
          rval = _halI2cErrToI2CResponse(((I2C_HandleTypeDef *)interface->handle)->ErrorCode);

#ifdef I2C_DEBUG
          printf("\n");
          printf("%s Error [%s] - %d\n", __func__, interface->name, rval);
#endif

#if I2C_WORKAROUND == 1
          i2cWorkaround(interface, rval);
#endif
          break;
        } else {
          rval = I2C_OK;
        }
      }

      // TODO - change this to allow for an i2c repeated start
      if(rxBuff != NULL && rxLen > 0) {
        hal_rval = HAL_I2C_Master_Receive(interface->handle, address << 1, rxBuff, rxLen, timeoutMs);
        if(hal_rval != HAL_OK) {
          rval = _halI2cErrToI2CResponse(((I2C_HandleTypeDef *)interface->handle)->ErrorCode);

#ifdef I2C_DEBUG
          printf("\n");
          printf("%s Error [%s] - %d\n", __func__, interface->name, rval);
#endif

#if I2C_WORKAROUND == 1
          i2cWorkaround(interface, rval);
#endif
          break;
        } else {
          rval = I2C_OK;
        }
      }

#ifdef I2C_DEBUG
      if(rxLen) {
        printf("RX(%u) ", rxLen);
        for(uint16_t idx = 0; idx < rxLen; idx++) {
          printf("%02X ", rxBuff[idx]);
        }
      }
      printf("\n");
#endif

    } while (0);

    xSemaphoreGive(interface->mutex);
  } else {
#ifdef I2C_DEBUG
    printf("%s Error [%s] - Unable to take mutex.\n", __func__, interface->name);
    rval = I2C_MUTEX;
#endif
  }

  return rval;
}

/*!
  I2CResponse_t i2cProbe(I2CInterface_t *interface, uint8_t address, uint32_t timeoutMs)
  \brief Check if an i2c device is present
  \param interface Handle to i2c interface
  \param address i2c device address
  \param timeoutMs timeout before giving up
  \return I2C_OK if device present
*/
I2CResponse_t i2cProbe(I2CInterface_t *interface, uint8_t address, uint32_t timeoutMs) {
  configASSERT(interface != NULL);

  // Make sure interface has been initialized!
  configASSERT(interface->mutex != NULL);

  I2CResponse_t rval = I2C_OK;
  if(xSemaphoreTake(interface->mutex, pdMS_TO_TICKS(timeoutMs)) == pdTRUE) {

    HAL_StatusTypeDef hal_rval;
    hal_rval = HAL_I2C_IsDeviceReady(interface->handle, address << 1, 1, timeoutMs);
    if(hal_rval != HAL_OK) {
      rval = _halI2cErrToI2CResponse(((I2C_HandleTypeDef *)interface->handle)->ErrorCode);

      // Ignore expected errors during a probe
      if ((rval != I2C_NACK) && (rval != I2C_TIMEOUT)) {
#ifdef I2C_DEBUG
        printf("%s Error [%s] - %d\n", __func__, interface->name, rval);
#endif
#if I2C_WORKAROUND == 1
        i2cWorkaround(interface, rval);
#endif
      }
    }

    xSemaphoreGive(interface->mutex);
  } else {
#ifdef I2C_DEBUG
    printf("%s Error [%s] - Unable to take mutex.\n", __func__, interface->name);
#endif
    rval = I2C_MUTEX;
  }

  return rval;
}
