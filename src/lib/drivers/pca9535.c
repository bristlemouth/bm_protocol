#include <stdbool.h>
#include "debug.h"
#include "gpioISR.h"
#include "pca9535.h"
#include "protected_i2c.h"
#include "task.h"
#include "task_priorities.h"

#define PCA9535_TIMEOUT_MS 10

typedef enum {
  INPUT_PORT_0 = 0,
  INPUT_PORT_1 = 1,
  OUTPUT_PORT_0 = 2,
  OUTPUT_PORT_1 = 3,
  POLARITY_INVERSION_PORT_0 = 4,
  POLARITY_INVERSION_PORT_1 = 5,
  CONFIGURATION_PORT_0 = 6,
  CONFIGURATION_PORT_1 = 7,
} PCA9535Register;

static TaskHandle_t PCA9535TaskHandle = NULL;

static bool pca9535Read(const void *pcaPinHandle, uint8_t *value);
static bool pca9535Write(const void *pcaPinHandle, uint8_t value);
static bool pca9535Configure(const void *pcaPinHandle, const void *config);
static bool pca9535RegisterCallback(const void *pcaPinHandle, const IOCallbackFn callback, void *config);

IODriver_t PCA9535PinDriver = {
  .write = pca9535Write,
  .read = pca9535Read,
  .config = pca9535Configure,
  .registerCallback = pca9535RegisterCallback
};

/*!
  Read a device register over i2c. All reads are done 2 bytes at a time to simplify logic

  \param device Handle to PCA9535 device
  \param reg register address
  \param *value pointer to variable to store value in
  \return true if successfull false otherwise
*/
static bool pca9535ReadReg(PCA9535Device_t *device, PCA9535Register reg, uint16_t *value) {
  configASSERT(device != NULL);
  configASSERT(value != NULL);

  I2CResponse_t result;
  uint8_t wBuff[] = {reg};

  result = i2cTxRx(device->i2cDeviceHandle,
                        device->address,
                        wBuff, sizeof(wBuff),
                        (uint8_t *)value, sizeof(uint16_t),
                        PCA9535_TIMEOUT_MS);

  return (result == I2C_OK);
}

/*!
  Write a device register over i2c. All writes are done 2 bytes at a time to simplify logic

  \param device Handle to PCA9535 device
  \param reg register address
  \param value register value
  \return true if successfull false otherwise
*/
static bool pca9535WriteReg(PCA9535Device_t *device, PCA9535Register reg, uint16_t value) {
  configASSERT(device != NULL);

  I2CResponse_t result;
  uint8_t wBuff[] = {reg, (value & 0xFF), (value >> 8)};
  result = i2cTxRx(device->i2cDeviceHandle,
                        device->address,
                        wBuff, sizeof(wBuff),
                        NULL, 0,
                        PCA9535_TIMEOUT_MS);

  return (result == I2C_OK);
}

/*!
  Initialize PCA9535 device by reading the current configuration and caching the values

  \param device Handle to PCA9535 device
  \return true if successfull false otherwise
*/
bool pca9535Init(PCA9535Device_t *device) {
  configASSERT(device != NULL);
  bool result = true;

  // Read configuration
  result &= pca9535ReadReg(device, CONFIGURATION_PORT_0, &device->configPort);

  // Initialize all outputs to 0
  result &= pca9535WriteReg(device, OUTPUT_PORT_0, 0);

  // Read output port
  result &= pca9535ReadReg(device, OUTPUT_PORT_0, &device->outputPort);

  // Read input port
  result &= pca9535ReadReg(device, INPUT_PORT_0, &device->inputPort);

  if(result != true) {
    printf("Error initializing PCA9535\n");
  } else {
    device->enabled = true;
  }

  return result;
}


/*!
  Internal function to read input register and check for any pin changes if needed

  \param device Handle to PCA9535 device
  \param isIRQ if true, interrupt callbacks will be called for evey pin change
  \return true if successfull false otherwise
*/
static bool pca9535UpdateInputs(PCA9535Device_t *device, bool isIRQ) {
  uint16_t inputPort = 0;
  bool result = pca9535ReadReg(device, INPUT_PORT_0, &inputPort);
  do {
    if(result != true ) {
      break;
    }

    uint16_t changes = device->inputPort ^ inputPort;

    // Update cached input port value
    device->inputPort = inputPort;

    // If there were no changes to inputs, we're done
    // We mask with config port so we don't care about output pins
    if(!(changes & device->configPort)) {
      break;
    }

    // If this isn't being called from an interrupt, don't call pin change callbacks
    if(!isIRQ) {
      break;
    }

    // Loop through all the pins and call callback functions where appropriate
    for(uint8_t pin = 0; pin < PCA9535_NUM_CHANNELS; pin++) {
      if(((changes >> pin) & 1) && (device->callbacks[pin] != NULL)) {
        // Ignoring args for now
        device->callbacks[pin](device, (inputPort >> pin) & 1, NULL);
      }
    }

  } while(0);
  return result;
}

/*!
  Read pin value

  \param pinHandle handle to pin to read
  \param *value pointer to variable in which the pin value is stored
  \return true if successfull false otherwise
*/
static bool pca9535Read(const void *pcaPinHandle, uint8_t *value) {
  const PCA9535Pin_t *pinHandle = (const PCA9535Pin_t *)pcaPinHandle;
  configASSERT(pinHandle != NULL);
  configASSERT(pinHandle->device != NULL);
  PCA9535Device_t *device = pinHandle->device;

  bool result = false;

  if(device->enabled) {
    result = pca9535UpdateInputs(device, false);
    if((result == true) && value != NULL) {
      *value = (device->inputPort >> pinHandle->pin) & 0x01;
    }
  }

  return result;
}


/*!
  Write value to pin

  \param pinHandle handle to pin to write to
  \param value value to write (0 or 1)
  \return true if successfull false otherwise
*/
static bool pca9535Write(const void *pcaPinHandle, uint8_t value) {
  const PCA9535Pin_t *pinHandle = (const PCA9535Pin_t *)pcaPinHandle;
  configASSERT(pinHandle != NULL);
  configASSERT(pinHandle->device != NULL);
  PCA9535Device_t *device = pinHandle->device;
  bool rval = false;

  if(device->enabled) {
    uint16_t outputPort;
    pca9535ReadReg(device, OUTPUT_PORT_0, &outputPort);
    if(device->outputPort != outputPort) {
      printf("Output port differs %04X %04X %04X\n", device->outputPort, outputPort, device->outputPort ^ outputPort);
    }

    if(value) {
      device->outputPort |= (1 << pinHandle->pin);
    } else {
      device->outputPort &= ~(1 << pinHandle->pin);
    }

    rval = pca9535WriteReg(device, OUTPUT_PORT_0, device->outputPort);
  }

  return rval;
}

/*!
  Configure pin as an input or output. Must be called for every pin separately

  \param pinHandle handle to pin to configure
  \param config pin configuration (ignored since the pin handle has that information)
  \return true if successfull false otherwise
*/
static bool pca9535Configure(const void *pcaPinHandle, const void *config) {
  const PCA9535Pin_t *pinHandle = (const PCA9535Pin_t *)pcaPinHandle;
  configASSERT(pinHandle != NULL);
  configASSERT(pinHandle->device != NULL);

  (void)config;

  PCA9535Device_t *device = pinHandle->device;

  bool rval = false;
  if(device->enabled){
    uint16_t configPort;
    pca9535ReadReg(device, CONFIGURATION_PORT_0, &configPort);
    if(device->configPort != configPort) {
      printf("Config port differs %04X %04X %04X\n", device->configPort, configPort, device->configPort ^ configPort);
    }

    if(pinHandle->mode == PCA9535_MODE_INPUT) {
      device->configPort |= (1 << pinHandle->pin);
    } else {
      device->configPort &= ~(1 << pinHandle->pin);
    }
    rval = pca9535WriteReg(device, CONFIGURATION_PORT_0, device->configPort);
    if(rval == true) {
      // update internal input state
      rval = pca9535UpdateInputs(device, false);
    }
  }
  return rval;
}

/*!
  Called from IO expander's interrupt pin service routine. Call from ISR!

  \param pcaPinHandle - ignored
  \param value - pin value (ignored)
  \param args - PCA device pointer sent as argument
  \return higherPriorityTaskWoken
*/
bool pca9535IRQHandler(const void *pcaPinHandle, uint8_t value, void *args) {
  configASSERT(args != NULL);
  const PCA9535Device_t *device = (const PCA9535Device_t *)args;

  (void)pcaPinHandle;
  (void) value;

  BaseType_t higherPriorityTaskWoken = false;
  if(PCA9535TaskHandle != NULL && device->enabled) {
    xTaskNotifyFromISR(PCA9535TaskHandle,(UBaseType_t)args, eSetValueWithOverwrite, &higherPriorityTaskWoken);
  }
  return higherPriorityTaskWoken;
}

/*!
  Register interrupt callback. Any GPIO change will trigger it for now.

  \param pinHandle handle to pin which this callback is for
  \param callback callback function to be called on value change
  \param args argument for callback function
  \return always true for now
*/
static bool pca9535RegisterCallback(const void *pcaPinHandle, const IOCallbackFn callback, void *args) {
  const PCA9535Pin_t *pinHandle = (const PCA9535Pin_t *)pcaPinHandle;
  configASSERT(pinHandle != NULL);
  configASSERT(pinHandle->device != NULL);

  PCA9535Device_t *device = pinHandle->device;
  configASSERT(pinHandle->pin < PCA9535_NUM_CHANNELS);
  (void) args; // Not used yet

  // If this was called through gpioISR, no need to use their callback,
  // since we'll already be outside interrupt context
  if(callback == gpioISRCallbackFromISR){
    device->callbacks[pinHandle->pin] = (IOCallbackFn)args;
  } else {
    device->callbacks[pinHandle->pin] = callback;
  }

  return true;
}

/*!
  PCA9535 IRQ Task


  All PCA9535 IRQ callbacks will be called from this task/context, NOT from
  interrupt context.
*/
static void pca9535IRQTask() {
  for(;;) {
    PCA9535Device_t *device;
    xTaskNotifyWait( 0x00, UINT32_MAX, (uint32_t *)&device, portMAX_DELAY );
    bool result;
    result = pca9535UpdateInputs(device, true);
    if(result != true ) {
      printf("PCA IRQ - Error reading from PCA\n");
    }
  }
}

/*!
  Trigger a PCA9535 update when the timer callback fires

  \param tmr timer this callback is for
  \return none
*/
static void pca9535PollTimerCallback(TimerHandle_t tmr){
  configASSERT(tmr);

  PCA9535Device_t *device = (PCA9535Device_t *)pvTimerGetTimerID(tmr);
  configASSERT(device);

  if(PCA9535TaskHandle){
    xTaskNotify(PCA9535TaskHandle,(UBaseType_t)device, eSetValueWithOverwrite);
  }
}

/*!
  Create a timer to poll IO expander for changes. (In case the interrupt line is not connected)

  \param device PCA device this timer is for
  \param intervalMs time between polls
  \return true if successful, false otherwise
*/
bool pca9535StartPollingTimer(PCA9535Device_t *device, uint32_t intervalMs) {
  configASSERT(device);

  bool rval = false;

  if(device->pollTimer == NULL) {
    device->pollTimer = xTimerCreate("pca9535 Poll",
                              pdMS_TO_TICKS(intervalMs),
                              pdTRUE,
                              device,
                              pca9535PollTimerCallback);
    configASSERT(device->pollTimer);

    configASSERT(xTimerStart(device->pollTimer, 10) == pdTRUE);

    rval = true;
  } else {
    printf("pca9535 poll timer already present!\n");
  }

  return rval;
}

/*!
  Stop an already running polling timer

  \param device PCA device this timer is for
  \return true if successful, false otherwise
*/
bool pca9535StopPollingTimer(PCA9535Device_t *device) {
  configASSERT(device);

  bool rval = false;

  if(device->pollTimer) {
    configASSERT(xTimerDelete(device->pollTimer, 10));
    device->pollTimer = NULL;
    rval = true;
  }

  return rval;
}

/*!
  Start IRQ task. Make sure stack is appropriately sized
*/
void pca9535StartIRQTask() {
  bool rval = xTaskCreate(
              pca9535IRQTask,
              "PCA9535_IRQ",
              128 * 2,
              NULL,
              PCA9535_IRQ_TASK_PRIORITY,
              &PCA9535TaskHandle);
  configASSERT(rval == pdTRUE);
}
