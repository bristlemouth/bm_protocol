#include <errno.h>
#include <string.h>

// Includes for FreeRTOS
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

#include "adi_bsp.h"
#include "app_util.h"
#include "bsp.h"
#include "gpioISR.h"
#include "io.h"
#include "l2.h"
#include "protected_spi.h"
#include "task_priorities.h"

#define RESET_DELAY (1)
#define AFTER_RESET_DELAY (100)

static adi_cb_t gpfSpiCallback = NULL;
static void *gpSpiCBParam = NULL;
extern adin_pins_t adin_pins;

//
// SPI callback (called at the end of adi_bsp_spi_write_and_read())
//
static void adi_bsp_spi_callback() {
  if (gpfSpiCallback) {
    (*gpfSpiCallback)(gpSpiCBParam, 0, NULL);
  } else {
    /* No SPI Callback function assigned */
  }
}

void adi_bsp_hw_reset() {

  IOWrite(adin_pins.reset, 0);
  vTaskDelay(pdMS_TO_TICKS(RESET_DELAY));
  IOWrite(adin_pins.reset, 1);
  vTaskDelay(pdMS_TO_TICKS(AFTER_RESET_DELAY));
}

/* SPI transceive wrapper */
uint32_t adi_bsp_spi_write_and_read(uint8_t *pBufferTx, uint8_t *pBufferRx, uint32_t nBytes,
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

  if (status == SPI_OK) {
    adi_bsp_spi_callback();
  } else {
    // TODO - handle this nicely
    configASSERT(0);
  }

  return status;
}

/* Setup the Interrupt on the DATA_RDY pin and create two threads to monitor SPI completion and
   Falling edge on DATA_RDY pin */
uint32_t adi_bsp_init(void) {

  IOWrite(adin_pins.chipSelect, 1);
  adi_bsp_hw_reset();

  return 0;
}

/* ADIN driver will call this (through the adi_hal layer)  to set up a callback for SPI TX
   completion */
uint32_t adi_bsp_spi_register_callback(L2IntCb const *pfCallback, void *const pCBParam) {
  /* TODO: When we switch to DMA, we need to register the callback */

  gpfSpiCallback = (L2IntCb)pfCallback;
  gpSpiCBParam = pCBParam;
  return 0;
}

/* ADIN driver will call this (through the adi_hal layer)  to set up a callback for DATA_RDY
   interrupts */
uint32_t adi_bsp_register_irq_callback(L2IntCb const *intCallback, void *hDevice) {
  IORegisterCallback(adin_pins.interrupt, bm_l2_handle_device_interrupt, NULL);
  bm_l2_register_interrupt_callback(intCallback, hDevice);
  return 0;
}
