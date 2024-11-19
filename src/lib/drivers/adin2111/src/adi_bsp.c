#include <errno.h>
#include <string.h>

// Includes for FreeRTOS
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

#include "app_util.h"
#include "bm_network_generic.h"
#include "bsp.h"
#include "gpioISR.h"
#include "io.h"
#include "protected_spi.h"
#include "task_priorities.h"

#define RESET_DELAY (1)
#define AFTER_RESET_DELAY (100)

static BmIntCb INT_CB = NULL;
extern adin_pins_t adin_pins;

/* SPI transceive wrapper */
BmErr bm_network_spi_read_write(uint8_t *pBufferTx, uint8_t *pBufferRx, uint32_t nBytes,
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

  return status == SPI_OK ? BmOK : BmEBADMSG;
}

bool network_device_interrupt(const void *pinHandle, uint8_t value, void *args) {
  (void)pinHandle;
  (void)value;
  (void)args;
  if (INT_CB) {
    return INT_CB() == BmOK;
  }
  return false;
}

/* ADIN driver will call this (through the adi_hal layer)  to set up a callback for DATA_RDY
   interrupts */
void bm_network_register_int_callback(BmIntCb cb) {
  INT_CB = cb;
  IORegisterCallback(adin_pins.interrupt, network_device_interrupt, NULL);
}
