#include <stdio.h>
#include "protected_spi.h"
#include "FreeRTOS.h"
#include "task.h"

#define MAX_NUM_SPI (3)

typedef struct SpiDmaContext {
  uint8_t num_registered_spi;
  SPI_HandleTypeDef *spi_handles[MAX_NUM_SPI];
  TaskHandle_t spi_task_to_wake[MAX_NUM_SPI];
  bool spi_dma_error_occurred[MAX_NUM_SPI];
} SpiDmaContext_t;

static SpiDmaContext_t _dma_context = {
  .num_registered_spi = 0,
  .spi_handles = {
    NULL,
    NULL,
    NULL,
  },
  .spi_task_to_wake = {
    NULL,
    NULL,
    NULL
  },
  .spi_dma_error_occurred = {
    false,
    false,
    false
  },
};

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

  configASSERT(_dma_context.num_registered_spi < MAX_NUM_SPI);
  interface->dma_id = _dma_context.num_registered_spi;
  _dma_context.spi_handles[interface->dma_id] = interface->handle;
  _dma_context.num_registered_spi++;

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
/*!
   Nonblocking Write and/or read data from spi device

  \param interface Handle to spi interface
  \param len spi transaction length
  \param txBuff tx buffer, set to NULL if unused
  \param rxBuff rx buffer, set to NULL if unused
  \param timeoutMs timeout before giving up
  \return SPIResponse depending on how it goes
*/
SPIResponse_t spiTxRxNonblocking(SPIInterface_t *interface, IOPinHandle_t *csPin, size_t len, uint8_t *txBuff, uint8_t *rxBuff, uint32_t timeoutMs) {
  configASSERT(interface != NULL);
  configASSERT((interface->dma_id > 0 && interface->dma_id < MAX_NUM_SPI));
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
    bool notified = false;
    do{
      _dma_context.spi_task_to_wake[interface->dma_id] = xTaskGetCurrentTaskHandle();

      // Assert CS if needed
      if(csPin != NULL) {
        IOWrite(csPin, 0);
      }

      if((txBuff != NULL) && (rxBuff != NULL)) {
        halRval = HAL_SPI_TransmitReceive_DMA(interface->handle, txBuff, rxBuff, len);
      } else if (txBuff != NULL) {
        halRval = HAL_SPI_Transmit_DMA(interface->handle, txBuff, len);
      } else if (rxBuff != NULL) {
        halRval = HAL_SPI_Receive_DMA(interface->handle, rxBuff, len);
      }
      
      if(halRval != HAL_OK){
        break;
      }

      notified = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(timeoutMs));

    } while(0);

    // De-assert CS if needed
    if(csPin != NULL) {
      IOWrite(csPin, 1);
    }

    switch(halRval) { // Check if the tx/rx initiation was succesfull.
      case HAL_OK: {
        if(!notified) { // Timeout occured.
          HAL_SPI_Abort(interface->handle); // Our task notification timed out, so try to HAL_SPI_Abort just in case.
          rval = SPI_TIMEOUT;
        } else if (_dma_context.spi_dma_error_occurred[interface->dma_id]){
          rval = SPI_ERR;
        } else {
          rval = SPI_OK;
        }
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

void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi) {
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  for(int i = 0; i < _dma_context.num_registered_spi; i++){
    if(hspi == _dma_context.spi_handles[i]){
      configASSERT(_dma_context.spi_task_to_wake[i]);
      vTaskNotifyGiveFromISR( _dma_context.spi_task_to_wake[i], &xHigherPriorityTaskWoken );
      _dma_context.spi_task_to_wake[i] = NULL;
      _dma_context.spi_dma_error_occurred[i] = false;
      break;
    }
  }
  portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
}

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi) {
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  for(int i = 0; i < _dma_context.num_registered_spi; i++){
    if(hspi == _dma_context.spi_handles[i]){
      configASSERT(_dma_context.spi_task_to_wake[i]);
      vTaskNotifyGiveFromISR( _dma_context.spi_task_to_wake[i], &xHigherPriorityTaskWoken );
      _dma_context.spi_task_to_wake[i] = NULL;
      _dma_context.spi_dma_error_occurred[i] = false;
      break;
    }
  }
  portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
}

void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi) {
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  for(int i = 0; i < _dma_context.num_registered_spi; i++){
    if(hspi == _dma_context.spi_handles[i]){
      configASSERT(_dma_context.spi_task_to_wake[i]);
      vTaskNotifyGiveFromISR( _dma_context.spi_task_to_wake[i], &xHigherPriorityTaskWoken );
      _dma_context.spi_task_to_wake[i] = NULL;
      _dma_context.spi_dma_error_occurred[i] = false;
      break;
    }
  }
  portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
}

void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi) {
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  for(int i = 0; i < _dma_context.num_registered_spi; i++){
    if(hspi == _dma_context.spi_handles[i]){
      configASSERT(_dma_context.spi_task_to_wake[i]);
      vTaskNotifyGiveFromISR( _dma_context.spi_task_to_wake[i], &xHigherPriorityTaskWoken );
      _dma_context.spi_task_to_wake[i] = NULL;
      _dma_context.spi_dma_error_occurred[i] = true;
      break;
    }
  }
  portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
}

void HAL_SPI_AbortCpltCallback(SPI_HandleTypeDef *hspi) {
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  for(int i = 0; i < _dma_context.num_registered_spi; i++){
    if(hspi == _dma_context.spi_handles[i]){
      configASSERT(_dma_context.spi_task_to_wake[i]);
      vTaskNotifyGiveFromISR( _dma_context.spi_task_to_wake[i], &xHigherPriorityTaskWoken );
      _dma_context.spi_task_to_wake[i] = NULL;
      _dma_context.spi_dma_error_occurred[i] = true;
      break;
    }
  }
  portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
}
