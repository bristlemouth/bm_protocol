#include "bsp.h"
#include "device/dcd.h"
#include "FreeRTOS.h"
#include "main.h"
#include "pcap.h"
#include "serial.h"
#include "serial_console.h"
#include "task.h"
#include "task_priorities.h"
#include "tusb.h"
#include "usb.h"
#include "app_util.h"

#include <stdio.h>

static TaskHandle_t usbTaskHandle;
static void usbTask(void *parameters);

static IOPinHandle_t *_vusbDetectPin;
static bool (*_usbIsConnectedFn)();

/*
  ISR VUSB detect
  \param[in] pinHandle - unused
  \param[in] value - value of vusb detect pin
  \param[in] args - unused

  \return higherPriorityTaskWoken
*/
bool vusbDetectIRQHandler(const void *pinHandle, uint8_t value, void *args) {
  (void)pinHandle;
  (void)args;
  BaseType_t higherPriorityTaskWoken = pdFALSE;

  if(value) {
    lpmPeripheralActiveFromISR(LPM_USB);
    dcd_connect(0);
  } else {
    lpmPeripheralInactiveFromISR(LPM_USB);
    dcd_disconnect(0);

    // Force disable usbCDC serial device
    serialConsoleDisable();

    // Force disable packet dumps
    pcapDisable();
  }

  return higherPriorityTaskWoken;
}

/*!
  Start USB task (tinyUSB does most of the work in a task context)
  \param[in] *vusbDetectPin (optional)

  \return true if successful false otherwise
*/
bool usbInit(IOPinHandle_t *vusbDetectPin, bool (*usbIsConnectedFn)()) {
  _vusbDetectPin = vusbDetectPin;
  _usbIsConnectedFn = usbIsConnectedFn;

  if(_vusbDetectPin){
    // Make sure we get notified when usb is connected/disconnected
    configASSERT(IORegisterCallback(_vusbDetectPin, vusbDetectIRQHandler, NULL));
  }

   BaseType_t rval = xTaskCreate(
    usbTask,
    "usb",
    configMINIMAL_STACK_SIZE * 4,
    NULL,
    USB_TASK_PRIORITY,
    &usbTaskHandle);

  configASSERT(rval == pdTRUE);
  return true;
}

/*!
  USB Interrupt handler for tinyUSB
*/
void OTG_FS_IRQHandler(void)
{
  tud_int_handler(0);
}


static uint8_t usbRxBuff[2048];

// TODO - put these in a central spot and have accessor functions
// such as serialHandleFromITF()
extern SerialHandle_t usbCLI;
extern SerialHandle_t usbPcap;

SerialHandle_t * serialHandleFromItf(uint8_t itf) {
  SerialHandle_t *handle;
  switch(itf) {
    case 0: {
      handle = &usbCLI;
      break;
    }
    case 1: {
      handle = &usbPcap;
      break;
    }
    default: {
      handle = NULL;
    }
  }

  return handle;
}

/*!
  Receive data over USB and send to rxStreamBuffer

  \param[in] itf - unused
*/
void tud_cdc_rx_cb(uint8_t itf) {

  // Get bytes from USB
  uint32_t count = tud_cdc_n_read(itf, usbRxBuff, sizeof(usbRxBuff));

  if(count) {
    SerialHandle_t *handle = serialHandleFromItf(itf);
    configASSERT(handle);

    // Add bytes to streambuffer
    size_t bytesSent = xStreamBufferSend(
                          handle->rxStreamBuffer,
                          usbRxBuff,
                          count,
                          100);

    if(bytesSent != count) {
      // Flag that some bytes were dropped
      handle->flags |= SERIAL_FLAG_RXDROP;

    }
  }
}

static uint8_t usbTxBuff[64];
/*!
  Callback when USB tx is complete. Sends more data if available, does
  nothing else otherwise.

  \param[in] itf - unused
*/
void tud_cdc_tx_complete_cb(uint8_t itf) {
  do {
    volatile uint32_t bytesToSend = tud_cdc_n_write_available(itf);
    if(!bytesToSend) {
      break;
    }

    SerialHandle_t *handle = serialHandleFromItf(itf);
    configASSERT(handle);

    handle->flags |= SERIAL_TX_IN_PROGRESS;

    bytesToSend = MIN(bytesToSend, sizeof(usbTxBuff));
    bytesToSend = serialGenericGetTxBytes(handle, usbTxBuff, bytesToSend);

    if(bytesToSend == 0) {
      handle->flags &= ~SERIAL_TX_IN_PROGRESS;
    }

    volatile uint32_t bytesSent = tud_cdc_n_write(itf, usbTxBuff, bytesToSend);

    // Why is this happening? bytesToSend2 > bytesToSend so this shouldn't happen
    // The fifo indices are sus
    configASSERT(bytesSent == bytesToSend);
    tud_cdc_n_write_flush(itf);

  } while(0);
}

/*!
  Main tinyUSB task to initialize and handle all events

  \param[in] *parameters unused
*/
static void usbTask(void *parameters) {
  (void)parameters;

  // Initialize tinyUSB stack
  tusb_init();

  // Disable usb if we're not connected on boot. This is needed because
  // tusb_init() calls dcd_init() in dcd_dwc2.c, which in turn calls
  // dcd_connect(), which enables the D+ pull-up resistor and may cause
  // issues if USB is not connected. See SC-183448 for more info
  //
  // If a usbIsConnectedFn is not provided, we assume that USB is always
  // connected
  if(_usbIsConnectedFn && !_usbIsConnectedFn()) {
    lpmPeripheralInactive(LPM_USB);
    dcd_disconnect(0);
  } else {
    lpmPeripheralActive(LPM_USB);
  }

  while(1) {
    // put this thread to waiting state until there are new events
    tud_task();

    // following code only run if tud_task() process at least 1 event
    tud_cdc_n_write_flush(0);
    tud_cdc_n_write_flush(1);
  }
}

/*!
  Clone of HAL_PCD_MspInit (src/bsp/<bsp_name>/Core/Src/usb_otg.c) without the PCD_HandleTypeDef stuff
  Sets up all of the USB GPIO's ad clocks
*/
void usbMspInit() {

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the peripherals clock
  */
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_CLK48;
    PeriphClkInit.IclkClockSelection = RCC_CLK48CLKSOURCE_HSI48;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
    {
      Error_Handler();
    }

    __HAL_RCC_GPIOA_CLK_ENABLE();
    /**USB_OTG_FS GPIO Configuration
    PA11     ------> USB_OTG_FS_DM
    PA12     ------> USB_OTG_FS_DP
    */
    GPIO_InitStruct.Pin = GPIO_PIN_11|GPIO_PIN_12;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF10_USB;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* USB_OTG_FS clock enable */
    __HAL_RCC_USB_CLK_ENABLE();

    /* Enable VDDUSB */
    if(__HAL_RCC_PWR_IS_CLK_DISABLED())
    {
      __HAL_RCC_PWR_CLK_ENABLE();
      HAL_PWREx_EnableVddUSB();
      __HAL_RCC_PWR_CLK_DISABLE();
    }
    else
    {
      HAL_PWREx_EnableVddUSB();
    }

    /* USB_OTG_FS interrupt Init */
    HAL_NVIC_SetPriority(OTG_FS_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(OTG_FS_IRQn);
}

/*!
  Weak function called whenever the USB line state changes. (connected/disconnected)
  Application can override as necessary

  \param[in] itf - unused
  \param[in] dtr - dtr signal
  \param[in] rts - rts signal

*/
__weak void usb_line_state_change(uint8_t itf, uint8_t dtr, bool rts) {
  (void) rts;
  printf("itf: %u RTS: %d DTR: %d\n", itf, rts, dtr);

  SerialHandle_t *handle = serialHandleFromItf(itf);
  configASSERT(handle);
  switch(itf) {
    case 0: {
      if ( dtr ) {
        // Terminal connected
        serialConsoleEnable();
      } else {
        // Terminal disconnected
        serialConsoleDisable();
      }
      break;
    }

    case 1: {
      if ( dtr ) {
        // Enable packet dumps
        pcapEnable();
      } else {
        // Disable packet dumps
        pcapDisable();
      }
      break;
    }
    default:
      configASSERT(0);
  }
}

/*!
  Callback when CDC line state changes
  Invoked when cdc when line state changed e.g connected/disconnected

  \param[in] itf - unused
  \param[in] dtr - dtr signal
  \param[in] rts - rts signal

*/
void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts)
{

  usb_line_state_change(itf, dtr, rts);

}

// Invoked when usb bus is suspended
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en) {
  (void)remote_wakeup_en;
  // serialConsoleDisable();
  printf("SUSPEND\n");
}
