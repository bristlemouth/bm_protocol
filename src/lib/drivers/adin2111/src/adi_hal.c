#include "adi_hal.h"
#include "FreeRTOS.h"
#include "app_util.h"
#include "bm_os.h"
#include "bsp.h"
#include "protected_spi.h"
#include "semphr.h"
#include "task_priorities.h"
#include "util.h"
#include <stdio.h>
#include <string.h>

#define adin_semaphore_max_queue 10

static HAL_Callback_t ADIN2111_MAC_SPI_CALLBACK = NULL;
static void *ADIN2111_MAC_SPI_CALLBACK_PARAM = NULL;
extern adin_pins_t adin_pins;
static BmTaskHandle ADI_SPI_TASK_HANDLE = NULL;

static void adi_spi_task(void *arg) {
  (void)arg;
  while (1) {
    if (ulTaskNotifyTake(pdFALSE, portMAX_DELAY) == pdTRUE) {
      if (ADIN2111_MAC_SPI_CALLBACK) {
        ADIN2111_MAC_SPI_CALLBACK(ADIN2111_MAC_SPI_CALLBACK_PARAM, 0, NULL);
      }
    }
  }
}

uint32_t HAL_EnterCriticalSection(void) {
  __disable_irq();
  return 0;
}

uint32_t HAL_ExitCriticalSection(void) {
  __enable_irq();
  return 0;
}

uint32_t HAL_DisableIrq(void) {
  NVIC_DisableIRQ(ADIN_INT_EXTI_IRQn);
  return ADI_HAL_SUCCESS;
}

uint32_t HAL_EnableIrq(void) {
  NVIC_EnableIRQ(ADIN_INT_EXTI_IRQn);
  return ADI_HAL_SUCCESS;
}

uint32_t HAL_GetPendingIrq(void) { return NVIC_GetPendingIRQ(ADIN_INT_EXTI_IRQn); }

uint32_t HAL_GetEnableIrq(void) { return NVIC_GetEnableIRQ(ADIN_INT_EXTI_IRQn); }

/*
 * @brief  Register SPI Callback function
 *
 * @param [in] spiCallback - Register SPI IRQ callback function
 * @param [in] hDevice - Pointer to Phy device handler
 * @param [out] none
 * @return none
 */

uint32_t HAL_Init_Hook(void) {
  BmErr err = BmENOMEM;

  err = bm_task_create(adi_spi_task, "ADIN SPI Task", 512, NULL, ADIN_SPI_TASK_PRIORITY,
                       &ADI_SPI_TASK_HANDLE);

  return err == BmOK ? ADI_HAL_SUCCESS : ADI_HAL_ERROR;
}

uint32_t HAL_UnInit_Hook(void) { return ADI_HAL_SUCCESS; }

// Required by the Analog Devices adi_hal.h to be implementeed by the application.
// The user will implement the bm_network_spi_read_write function in their application.
uint32_t HAL_SpiReadWrite(uint8_t *pBufferTx, uint8_t *pBufferRx, uint32_t nBytes,
                          bool useDma) {
  SPIResponse_t status = SPI_OK;

  //
  // spi3 has a limit of 1024 bytes per transaction
  // See Table 579 on 59.3 of the reference manual (RM0456)
  // Manually controlling CS and "emulating" larger transactions here
  //

  IOWrite(adin_pins.chipSelect, 0);
  // NOTE: We cannot send 1024 bytes, since the counter will wrap and be zero and return an error
  // Sending more than 1024 bytes will send nBuytes % 1024 then time out
  for (uint32_t idx = 0; idx < nBytes; idx += 1023) {
    uint32_t bytesRemaining = nBytes - idx;
    uint32_t subNbytes = MIN(bytesRemaining, 1023);
    if (useDma) {
      status = spiTxRxNonblocking(adin_pins.spiInterface, NULL, subNbytes, &pBufferTx[idx],
                                  &pBufferRx[idx],
                                  100); // TODO: Figure out timeout value. Set to 100 for now?
    } else {
      status =
          spiTxRx(adin_pins.spiInterface, NULL, subNbytes, &pBufferTx[idx], &pBufferRx[idx],
                  100); // TODO: Figure out timeout value. Set to 100 for now?
    }
    if (status != SPI_OK) {
      break;
    }
  }
  IOWrite(adin_pins.chipSelect, 1);

  if (status == SPI_OK && ADI_SPI_TASK_HANDLE) {
    xTaskNotifyGive(ADI_SPI_TASK_HANDLE);
  } else {
    printf("Network SPI Read/Write Failed\n");
  }

  return status;
}

// Required by the Analog Devices adi_hal.h to be implementeed by the application.
// We save the pointer here in our driver wrapper to simplify Bristlemouth integration.
uint32_t HAL_SpiRegisterCallback(HAL_Callback_t const *spiCallback, void *hDevice) {
  // Analog Devices code has a bug at adi_mac.c:535 where they
  // cast a function pointer to a function pointer pointer incorrectly.
  ADIN2111_MAC_SPI_CALLBACK = (const HAL_Callback_t)spiCallback;
  ADIN2111_MAC_SPI_CALLBACK_PARAM = hDevice;
  return ADI_ETH_SUCCESS;
}
