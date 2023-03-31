#include <string.h>
#include <errno.h>
#include <string.h>

// Includes for FreeRTOS
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "adi_bsp.h"
#include "bsp.h"
#include "gpioISR.h"
#include "io.h"
#include "protected_spi.h"
#include "task_priorities.h"
#include "util.h"

#define RESET_DELAY       (1)
#define AFTER_RESET_DELAY (100)

#ifdef BSP_NUCLEO_U575
#define ADIN_NSS ADIN_CS
#define ADIN_INT ADIN_RDY
#endif // BSP_DEV_MOTE_V1_0

static adi_cb_t gpfIntCallback = NULL;
static void *gpIntCBParam = NULL;

static adi_cb_t gpfSpiCallback = NULL;
static void *gpSpiCBParam = NULL;

static adi_irq_evt_t _irq_evt_cb = NULL;

static bool adin_bsp_gpio_callback_fromISR(const void *pinHandle, uint8_t value, void *args);

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

//
// IRQ callback (called from eth_adin2111 event task)
//
void adi_bsp_irq_callback() {
  if (gpfIntCallback) {
    (*gpfIntCallback)(gpIntCBParam, 0, NULL);
  } else {
    /* No GPIO Callback function assigned */
  }
}

//
// ADIN GPIO ISR callback (All calls must be ISR safe!)
//
static bool adin_bsp_gpio_callback_fromISR(const void *pinHandle, uint8_t value, void *args) {

  (void) args;
  (void) pinHandle;
  (void) value;

  BaseType_t xHigherPriorityTaskWoken = pdFALSE;

  if(_irq_evt_cb) {
    _irq_evt_cb(&xHigherPriorityTaskWoken);
  }

  return xHigherPriorityTaskWoken;
}

void adi_bsp_hw_reset() {

  IOWrite(adin_pins.reset, 0);
  vTaskDelay(pdMS_TO_TICKS(RESET_DELAY));
  IOWrite(adin_pins.reset, 1);
  vTaskDelay(pdMS_TO_TICKS(AFTER_RESET_DELAY));
}

/* SPI transceive wrapper */
uint32_t adi_bsp_spi_write_and_read(uint8_t *pBufferTx, uint8_t *pBufferRx, uint32_t nBytes, bool useDma) {

  SPIResponse_t status = SPI_OK;

// TODO - make configurable spi bus instead of #defines
#ifdef BSP_NUCLEO_U575
  (void) useDma;
  status = spiTxRx(adin_pins.spiInterface, adin_pins.chipSelect, nBytes, pBufferTx, pBufferRx, 100); // TODO: Figure out timeout value. Set to 100 for now?
#else

// Skip DMA for now since we haven't enabled it on this BSP
#ifdef BSP_DEV_MOTE_HYDROPHONE
  useDma = false;
#endif

  //
  // spi3 has a limit of 1024 bytes per transaction
  // See Table 579 on 59.3 of the reference manual (RM0456)
  // Manually controlling CS and "emulating" larger transactions here
  //

  IOWrite(adin_pins.chipSelect, 0);
  // NOTE: We cannot send 1024 bytes, since the counter will wrap and be zero and return an error
  // Sending more than 1024 bytes will send nBuytes % 1024 then time out
  for(uint32_t idx = 0; idx < nBytes; idx += 1023) {
    uint32_t bytesRemaining = nBytes - idx;
    uint32_t subNbytes = MIN(bytesRemaining, 1023);
    if(useDma){
        status = spiTxRxNonblocking(adin_pins.spiInterface, NULL, subNbytes, &pBufferTx[idx], &pBufferRx[idx], 100); // TODO: Figure out timeout value. Set to 100 for now?
    } else {
        status = spiTxRx(adin_pins.spiInterface, NULL, subNbytes, &pBufferTx[idx], &pBufferRx[idx], 100); // TODO: Figure out timeout value. Set to 100 for now?
    }
    if(status != SPI_OK) {
      break;
    }
  }
  IOWrite(adin_pins.chipSelect, 1);

#endif

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
uint32_t adi_bsp_spi_register_callback(adi_cb_t const *pfCallback, void *const pCBParam) {
  /* TODO: When we switch to DMA, we need to register the callback */

  gpfSpiCallback = (adi_cb_t) pfCallback;
  gpSpiCBParam = pCBParam ;
  return 0;
}

/* ADIN driver will call this (through the adi_hal layer)  to set up a callback for DATA_RDY
   interrupts */
uint32_t adi_bsp_register_irq_callback(adi_cb_t const *intCallback, void * hDevice) {
  IORegisterCallback(adin_pins.interrupt, adin_bsp_gpio_callback_fromISR, NULL);
  gpfIntCallback = (adi_cb_t) intCallback;
  gpIntCBParam = hDevice;
  return 0;
}

void adi_bsp_register_irq_evt (adi_irq_evt_t irq_evt_cb) {
  _irq_evt_cb = irq_evt_cb;
}
