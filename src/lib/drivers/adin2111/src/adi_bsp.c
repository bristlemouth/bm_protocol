#include <string.h>
#include <errno.h>
#include <string.h>

// Includes for FreeRTOS
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "gpioISR.h"
#include "adi_bsp.h"
#include "io.h"
#include "bsp.h"
#include "protected_spi.h"

#define RESET_DELAY       (1)
#define AFTER_RESET_DELAY (100)

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

    SPIResponse_t status = spiTxRx(&spi1, &ADIN_CS, nBytes, pBufferTx, pBufferRx, 100); // TODO: Figure out timeout value. Set to 100 for now?
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

    adi_bsp_hw_reset();

    BaseType_t rval;
    rval = xTaskCreate(adin_bsp_spi_thread,
                       "ADIN_SPI",
                       1024,
                       NULL,
                       10,
                       &spiTask);
    configASSERT(rval == pdTRUE);

    rval = xTaskCreate(adin_bsp_gpio_thread,
                       "ADIN_IO",
                       1024,
                       NULL,
                       10,
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
    IORegisterCallback(&ADIN_RDY, adin_bsp_gpio_callback, NULL);
    gpfIntCallback = (adi_cb_t) intCallback;
    gpIntCBParam = hDevice;
    return 0;
}

