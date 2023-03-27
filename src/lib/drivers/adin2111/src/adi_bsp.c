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

// static void adin_bsp_spi_thread(void *parameters);
static void adin_bsp_gpio_thread(void *parameters);
static bool adin_bsp_gpio_callback(const void *pinHandle, uint8_t value, void *args);

/* Handles for the tasks create by main(). */
static TaskHandle_t spiTask = NULL;
static TaskHandle_t gpioTask = NULL;

/* Thread to respond to SPI interrupts */
static void adin_bsp_spi_thread(void *parameters) {
  (void) parameters;

  while(1) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    if (gpfSpiCallback) {
      (*gpfSpiCallback)(gpSpiCBParam, 0, NULL);
    } else {
      /* No SPI Callback function assigned */
    }
  }
}

/* Thread to respond to GPIO interrupts */
static void adin_bsp_gpio_thread(void *parameters) {
  (void) parameters;

  while(1) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    if (gpfIntCallback) {
      (*gpfIntCallback)(gpIntCBParam, 0, NULL);
    } else {
      /* No GPIO Callback function assigned */
    }
  }
}

static bool adin_bsp_gpio_callback(const void *pinHandle, uint8_t value, void *args) {

  (void) args;
  (void) pinHandle;
  (void) value;

  xTaskNotifyGive(gpioTask);
  return true; // TODO: What does this bool mean?
}

void adi_bsp_hw_reset() {

  IOWrite(&ADIN_RST, 0);
  vTaskDelay(pdMS_TO_TICKS(RESET_DELAY));
  IOWrite(&ADIN_RST, 1);
  vTaskDelay(pdMS_TO_TICKS(AFTER_RESET_DELAY));
}

void adi_bsp_int_n_set_pending_irq(void) {
  xTaskNotifyGive(gpioTask);
}

/* SPI transceive wrapper */
uint32_t adi_bsp_spi_write_and_read(uint8_t *pBufferTx, uint8_t *pBufferRx, uint32_t nBytes, bool useDma) {
  (void) useDma;

  SPIResponse_t status = SPI_OK;

// TODO - make configurable spi bus instead of #defines
#ifdef BSP_NUCLEO_U575
  status = spiTxRx(&spi1, &ADIN_CS, nBytes, pBufferTx, pBufferRx, 100); // TODO: Figure out timeout value. Set to 100 for now?
#else

  //
  // spi3 has a limit of 1024 bytes per transaction
  // See Table 579 on 59.3 of the reference manual (RM0456)
  // Manually controlling CS and "emulating" larger transactions here
  //

  IOWrite(&ADIN_CS, 0);
  // NOTE: We cannot send 1024 bytes, since the counter will wrap and be zero and return an error
  // Sending more than 1024 bytes will send nBuytes % 1024 then time out
  for(uint32_t idx = 0; idx < nBytes; idx += 1023) {
    uint32_t bytesRemaining = nBytes - idx;
    uint32_t subNbytes = MIN(bytesRemaining, 1023);
    status = spiTxRx(&spi3, NULL, subNbytes, &pBufferTx[idx], &pBufferRx[idx], 100); // TODO: Figure out timeout value. Set to 100 for now?
  }
  IOWrite(&ADIN_CS, 1);

#endif
  if (status == SPI_OK) {
    /* Give semaphore to allow SPI Thread to call appropriate callback */
    xTaskNotifyGive(spiTask);
  } else {
    configASSERT(0);
  }

  return status;
}

/* Setup the Interrupt on the DATA_RDY pin and create two threads to monitor SPI completion and
   Falling edge on DATA_RDY pin */
uint32_t adi_bsp_init(void) {

  // Power up adin
#ifndef BSP_NUCLEO_U575

  IOWrite(&ADIN_PWR, 1);
  vTaskDelay(10); // huge delay
  IOWrite(&ADIN_CS, 1);
#endif

  adi_bsp_hw_reset();

  BaseType_t rval;
  rval = xTaskCreate(adin_bsp_spi_thread,
             "ADIN_SPI",
             1024,
             NULL,
             ADIN_SPI_TASK_PRIORITY,
             &spiTask);
  configASSERT(rval == pdTRUE);

  rval = xTaskCreate(adin_bsp_gpio_thread,
             "ADIN_IO",
             1024,
             NULL,
             ADIN_GPIO_TASK_PRIORITY,
             &gpioTask);
  configASSERT(rval == pdTRUE);

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
  IORegisterCallback(&ADIN_INT, adin_bsp_gpio_callback, NULL);
  gpfIntCallback = (adi_cb_t) intCallback;
  gpIntCBParam = hDevice;
  return 0;
}
