#include "protected_i2c.h"
#include <stdio.h>

struct I2CCtx {
  I2CInterface_t *interface[2];
  uint8_t count;
};

#define I2C_NOTIFY_OK (0)
#define I2C_NOTIFY_ERROR (1 << 0)
#define I2C_NOTIFY_ABORT (1 << 1)

static struct I2CCtx ctx = {0};

// Translate HAL i2c error codes to ours
static I2CResponse_t _halI2cErrToI2CResponse(uint32_t errorCode) {
  I2CResponse_t rval = I2C_ERR; // Generic error

  if (errorCode & HAL_I2C_ERROR_AF) {
    rval = I2C_NACK;
  } else if (errorCode & HAL_I2C_ERROR_TIMEOUT) {
    rval = I2C_TIMEOUT;
  } else if (errorCode == 0) {
    rval = I2C_OK;
  }

  return rval;
}

// Sometimes the i2c bus gets wedged in a state where all transactions fail
// While we figure out the root cause, re-initializing the interface
// seems to clear the problem
#if I2C_WORKAROUND == 1
static void i2cWorkaround(I2CInterface_t *interface, I2CResponse_t rval) {
  if (rval == I2C_TIMEOUT || rval == I2C_ERR) {
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

  ctx.interface[ctx.count++] = interface;

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
I2CResponse_t i2cTxRx(I2CInterface_t *interface, uint8_t address, uint8_t *txBuff, size_t txLen,
                      uint8_t *rxBuff, size_t rxLen, uint32_t timeoutMs) {
  configASSERT(interface != NULL);
  I2CResponse_t rval = I2C_ERR;

  // Make sure interface has been initialized!
  configASSERT(interface->mutex != NULL);

  if (xSemaphoreTake(interface->mutex, pdMS_TO_TICKS(timeoutMs)) == pdTRUE) {

    if (interface->lpm_mask) {
      lpmPeripheralActive(interface->lpm_mask);
    }

#ifdef I2C_DEBUG
    printf("%s [%s] ", __func__, interface->name);
    if (txLen) {
      printf("TX(%" PRIu32 ") ", txLen);
      for (uint16_t idx = 0; idx < txLen; idx++) {
        printf("%02X ", txBuff[idx]);
      }
    }
#endif

    HAL_StatusTypeDef hal_rval;
    do {
      if (txBuff != NULL && txLen > 0) {
        hal_rval =
            HAL_I2C_Master_Transmit(interface->handle, address << 1, txBuff, txLen, timeoutMs);
        if (hal_rval != HAL_OK) {
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
      if (rxBuff != NULL && rxLen > 0) {
        hal_rval =
            HAL_I2C_Master_Receive(interface->handle, address << 1, rxBuff, rxLen, timeoutMs);
        if (hal_rval != HAL_OK) {
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
      if (rxLen) {
        printf("RX(%" PRIu32 ") ", rxLen);
        for (uint16_t idx = 0; idx < rxLen; idx++) {
          printf("%02X ", rxBuff[idx]);
        }
      }
      printf("\n");
#endif

    } while (0);

    if (interface->lpm_mask) {
      lpmPeripheralInactive(interface->lpm_mask);
    }

    xSemaphoreGive(interface->mutex);
  } else {
#ifdef I2C_DEBUG
    printf("%s Error [%s] - Unable to take mutex.\n", __func__, interface->name);
    rval = I2C_MUTEX;
#endif
  }

  return rval;
}

static I2CResponse_t handle_errors_and_wait(I2CInterface_t *interface, uint32_t timeout_ms,
                                            HAL_StatusTypeDef hal_rval) {
  I2CResponse_t ret = I2C_ERR;

  if (hal_rval != HAL_OK) {
    ret = _halI2cErrToI2CResponse(((I2C_HandleTypeDef *)interface->handle)->ErrorCode);

#if I2C_WORKAROUND == 1
    i2cWorkaround(interface, ret);
#endif

    return ret;
  }

  uint32_t task_notify_flag = 0;
  BaseType_t notified =
      xTaskNotifyWait(pdTRUE, UINT32_MAX, &task_notify_flag, pdMS_TO_TICKS(timeout_ms));
  if (notified != pdPASS) {
    ret = I2C_TIMEOUT;
  } else if (task_notify_flag) {
    ret = I2C_ERR;
  } else {
    ret = I2C_OK;
  }

  return ret;
}

/*!
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
I2CResponse_t i2cTxRxNonblocking(I2CInterface_t *interface, uint8_t address, uint8_t *txBuff,
                                 size_t txLen, uint8_t *rxBuff, size_t rxLen,
                                 uint32_t timeoutMs) {
  configASSERT(interface != NULL);
  // Make sure interface has been initialized!
  configASSERT(interface->mutex != NULL);

  if (xSemaphoreTake(interface->mutex, pdMS_TO_TICKS(timeoutMs)) != pdTRUE) {
    return I2C_TIMEOUT;
  }

  I2CResponse_t rval = I2C_OK;
  HAL_StatusTypeDef hal_rval;
  interface->task = xTaskGetCurrentTaskHandle();

  if (interface->lpm_mask) {
    lpmPeripheralActive(interface->lpm_mask);
  }

  if (txBuff != NULL && txLen > 0) {
    hal_rval = HAL_I2C_Master_Transmit_DMA(interface->handle, address << 1, txBuff, txLen);
    rval = handle_errors_and_wait(interface, timeoutMs, hal_rval);
  }

  if (rval == I2C_OK && rxBuff != NULL && rxLen > 0) {
    hal_rval = HAL_I2C_Master_Receive_DMA(interface->handle, address << 1, rxBuff, rxLen);
    rval = handle_errors_and_wait(interface, timeoutMs, hal_rval);
  }

  if (interface->lpm_mask) {
    lpmPeripheralInactive(interface->lpm_mask);
  }

  interface->task = NULL;

  xSemaphoreGive(interface->mutex);

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
  if (xSemaphoreTake(interface->mutex, pdMS_TO_TICKS(timeoutMs)) == pdTRUE) {

    if (interface->lpm_mask) {
      lpmPeripheralActive(interface->lpm_mask);
    }

    HAL_StatusTypeDef hal_rval;
    hal_rval = HAL_I2C_IsDeviceReady(interface->handle, address << 1, 1, timeoutMs);
    if (hal_rval != HAL_OK) {
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

    if (interface->lpm_mask) {
      lpmPeripheralInactive(interface->lpm_mask);
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

static inline void i2c_dma_complete(I2C_HandleTypeDef *hi2c, BaseType_t notify_flags) {
  BaseType_t higher_priority_task_woken = pdFALSE;
  for (uint8_t i = 0; i < ctx.count; i++) {
    if (hi2c == ctx.interface[i]->handle && ctx.interface[i]->task) {
      xTaskNotifyFromISR(ctx.interface[i]->task, notify_flags, eSetValueWithOverwrite,
                         &higher_priority_task_woken);
    }
  }
  portYIELD_FROM_ISR(higher_priority_task_woken);
}

void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef *hi2c) {
  i2c_dma_complete(hi2c, I2C_NOTIFY_OK);
}

void HAL_I2C_MasterRxCpltCallback(I2C_HandleTypeDef *hi2c) {
  i2c_dma_complete(hi2c, I2C_NOTIFY_OK);
}

void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c) {
  i2c_dma_complete(hi2c, I2C_NOTIFY_ERROR);
}

void HAL_I2C_AbortCpltCallback(I2C_HandleTypeDef *hi2c) {
  i2c_dma_complete(hi2c, I2C_NOTIFY_ABORT);
}
