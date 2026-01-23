#include "bno085.h"
#include "uptime.h"
#include "debug.h"
#include "bsp.h"
#include "string.h"
#include "watchdog.h"

// Initialize static instance pointer
Bno085* Bno085::_instance = nullptr;

Bno085::Bno085(I2CInterface_t* i2cInterface, uint8_t address)
    : _interface(i2cInterface),
      _addr(address),
      _serviceTaskHandle(NULL),
      _i2cMutex(NULL),
      _eventCallback(NULL),
      _sensorCallback(NULL),
      _initialized(false) {
    _instance = this;
}

bool Bno085::init(sh2_EventCallback_t *eventCallback,
                  sh2_SensorCallback_t *sensorCallback) {
    _eventCallback = eventCallback;
    _sensorCallback = sensorCallback;

    // Create mutex for I2C access
    _i2cMutex = xSemaphoreCreateMutex();
    if (!_i2cMutex) {
        return false;
    }

    // Create the service task
    BaseType_t ret = xTaskCreate(
        serviceTask,
        "BNO085",
        configMINIMAL_STACK_SIZE * 4,
        this,
        21,  // High priority to service sensor quickly
        &_serviceTaskHandle
    );

    return ret == pdPASS;
}

void Bno085::notifyDataReady() {
    // Called from ISR when INT pin goes low (data ready)
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (_serviceTaskHandle) {
        vTaskNotifyGiveFromISR(_serviceTaskHandle, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

void Bno085::serviceTask(void *arg) {
    Bno085* instance = static_cast<Bno085*>(arg);

    // Setup HAL
    sh2_Hal_t hal;
    hal.open = Bno085::open;
    hal.close = Bno085::close;
    hal.read = Bno085::read;
    hal.write = Bno085::write;
    hal.getTimeUs = Bno085::getTimeUs;

    // Open sensor hub
    int status = sh2_open(&hal, instance->_eventCallback, NULL);
    if (status != SH2_OK) {
        printf("BNO085: Failed to open sensor hub: %d\n", status);
        vTaskDelete(NULL);
        return;
    }

    printf("BNO085: Sensor hub opened successfully\n");

    // Set sensor callback
    if (instance->_sensorCallback) {
        sh2_setSensorCallback(instance->_sensorCallback, NULL);
    }

    instance->_initialized = true;

     // Main service loop - wait for INT pin notification
    while (1) {
      // Wait for notification from ISR (INT pin asserted)
      // Timeout after 100ms to periodically check even if no interrupt
      uint32_t notificationValue = ulTaskNotifyTake(pdTRUE, 1000);
      (void)notificationValue;
    //   printf("counter: %" PRIu32 "\n", counter);
      uint8_t intPinState = 0;
      IORead(&IOEXP_INT, &intPinState);
      watchdogFeed();
        // printf("service started, pinstate: %" PRIu8 ", uptime: %" PRIu64 "\n", intPinState, uptimeGetMicroSeconds());
            //, pinstate: %" PRIu8 "\n", intPinState);
    //   if (notificationValue > 0) {
    //     printf("got pin notification\n");
    //   }
        // INT pin was asserted (went LOW), data is ready
            // Keep servicing until INT goes back HIGH
            // do {
                sh2_service();

                // Check if INT pin is still LOW (more data available)
                // IORead(&IOEXP_INT, &intPinState);

            // } while (intPinState == 0);  // Continue while INT is LOW
    //         printf("pin high\n");
    //   } else {
    //     printf("checking if we need to service\n");
    //     // Timeout - call sh2_service() anyway for housekeeping
    //     // This handles cases where we might have missed an interrupt
    //     // while (intPinState == 1) {
    //         sh2_service();
    //         IORead(&IOEXP_INT, &intPinState);
    //     // }
        // printf("service ended, pinstate: %" PRIu8 "\n", intPinState);
    //   }
    }
}

bool Bno085::configureSensor(sh2_SensorId_t sensorId, uint32_t reportInterval_us) {
    if (!_initialized) {
        return false;
    }

    sh2_SensorConfig_t config;
    config.changeSensitivityEnabled = false;
    config.wakeupEnabled = false;
    config.changeSensitivityRelative = false;
    config.alwaysOnEnabled = false;
    config.changeSensitivity = 0;
    config.reportInterval_us = reportInterval_us;
    config.batchInterval_us = 0;

    int status = sh2_setSensorConfig(sensorId, &config);
    if (status != SH2_OK) {
        printf("BNO085: Failed to configure sensor %d: %d\n", sensorId, status);
        return false;
    }

    return true;
}

int Bno085::open(sh2_Hal_t *self) {
    (void)self;
    // Nothing special needed for I2C
    return SH2_OK;
}

void Bno085::close(sh2_Hal_t *self) {
    (void)self;
    // Nothing special needed
}

int Bno085::read(sh2_Hal_t *self, uint8_t *pBuffer, unsigned len, uint32_t *t_us) {
    (void)self;

    // Use the static instance pointer
    if (!_instance) {
        return 0;
    }

    return _instance->readBytes(pBuffer, len, t_us);
}

int Bno085::write(sh2_Hal_t *self, uint8_t *pBuffer, unsigned len) {
    (void)self;

    // Use the static instance pointer
    if (!_instance) {
        return 0;
    }

    return _instance->writeBytes(pBuffer, len);
}

uint32_t Bno085::getTimeUs(sh2_Hal_t *self) {
    (void)self;
    return uptimeGetMs() * 1000;
}

// Instance methods that do the actual work
int Bno085::readBytes(uint8_t *pBuffer, unsigned len, uint32_t *t_us) {
    if (xSemaphoreTake(_i2cMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return 0;
    }

    int bytesRead = 0;

    // // Read SHTP header (4 bytes)
    // uint8_t header[4];
    // if (i2cTxRx(_interface, _addr, NULL, 0, header, 4, 100) == 0) {

    //     // Parse cargo length from header
    //     uint16_t cargoLength = (header[0] | (header[1] << 8)) & 0x7FFF;

    //     if (cargoLength > 0 && cargoLength <= len) {
    //         // Copy header to output buffer
    //         memcpy(pBuffer, header, 4);

    //         // Read remaining cargo if any
    //         if (cargoLength > 4) {
    //             if (i2cTxRx(_interface, _addr, NULL, 0,
    //                        pBuffer + 4, cargoLength - 4, 100) == 0) {
    //                 bytesRead = cargoLength;
    //             }
    //         } else {
    //             bytesRead = 4;
    //         }

    //         // Set timestamp
    //         if (t_us) {
    //             *t_us = uptimeGetMs() * 1000;
    //         }
    //     }
    // }

    // Read maximum SHTP packet size in ONE I2C transaction
    // The BNO085 will provide the actual length in the header
    // SH2_HAL_MAX_TRANSFER_IN is typically 512 bytes
    const uint16_t maxReadSize = (len < SH2_HAL_MAX_TRANSFER_IN) ? len : SH2_HAL_MAX_TRANSFER_IN;

    // Single I2C read transaction - this is key!
    if (i2cTxRx(_interface, _addr, NULL, 0, pBuffer, maxReadSize, 100) == 0) {

        // Now parse the header from what we just read
        uint16_t cargoLength = (pBuffer[0] | (pBuffer[1] << 8)) & 0x7FFF;

        if (cargoLength > 0 && cargoLength <= maxReadSize) {
            // We got valid data
            bytesRead = cargoLength;

            // Set timestamp
            if (t_us) {
                *t_us = uptimeGetMs() * 1000;
            }
        }
        // If cargoLength is 0, no data was available - return 0
    }


    xSemaphoreGive(_i2cMutex);
    return bytesRead;
}

int Bno085::writeBytes(uint8_t *pBuffer, unsigned len) {
    if (xSemaphoreTake(_i2cMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return 0;
    }

    int bytesWritten = 0;
    if (i2cTxRx(_interface, _addr, pBuffer, len, NULL, 0, 100) == 0) {
        bytesWritten = len;
    }

    xSemaphoreGive(_i2cMutex);
    return bytesWritten;
}

extern "C" void bno085NotifyDataReadyFromISR(void) {
    if (Bno085::_instance) {
        Bno085::_instance->notifyDataReady();
    }
}

// Bno085::Bno085(I2CInterface_t* i2cInterface, uint8_t address) {
//   _interface = i2cInterface;
//   _addr = static_cast<uint8_t>(address);
// }

// bool Bno085::init() {
//   return true;
// }

// int Bno085::open(sh2_Hal_t *self) {
//   (void) self;
//   // TODO - this is what needs to happen here:

//   // This function initializes communications with the device.  It
//   // can initialize any GPIO pins and peripheral devices used to
//   // interface with the sensor hub.
//   // It should also perform a reset cycle on the sensor hub to
//   // ensure communications start from a known state.

//   // My notes:
//   // I believe since we always have the bus on we just need to
//   // do the reset portion of above

//   // I'm not sure what needs to be returned here for success/fail
//   return SH2_OK;
// }

// void Bno085::close(sh2_Hal_t *self) {
//   (void) self;
//   // TODO:
//   // This function completes communications with the sensor hub.
//   // It should put the device in reset then de-initialize any
//   // peripherals or hardware resources that were used.

//   // My notes:
//   // Since we are not actually turning off the HAL/I2C bus here
//   // I think maybe we just use a private variable to block tx/rx
//   // from the sensor
// }

// int Bno085::read(sh2_Hal_t *self, uint8_t *pBuffer, unsigned len, uint32_t *t_us) {
//   (void) self;
//   (void)t_us;

//   Bno085* instance = static_cast<Bno085*>(self->user);

//     if (xSemaphoreTake(instance->_i2cMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
//         return 0;
//     }

//     int bytesRead = 0;

//     // Read SHTP header (4 bytes)
//     uint8_t header[4];
//     if (i2cTxRx(instance->_interface, instance->_addr, NULL, 0,
//                 header, 4, 100) == 0) {

//         // Parse cargo length from header
//         uint16_t cargoLength = (header[0] | (header[1] << 8)) & 0x7FFF;

//         if (cargoLength > 0 && cargoLength <= len) {
//             // Copy header to output buffer
//             memcpy(pBuffer, header, 4);

//             // Read remaining cargo if any
//             if (cargoLength > 4) {
//                 if (i2cTxRx(instance->_interface, instance->_addr, NULL, 0,
//                            pBuffer + 4, cargoLength - 4, 100) == 0) {
//                     bytesRead = cargoLength;
//                 }
//             } else {
//                 bytesRead = 4;
//             }

//             // Set timestamp
//             if (t_us) {
//                 *t_us = uptimeGetMs() * 1000;
//             }
//         }
//     }

//     xSemaphoreGive(instance->_i2cMutex);
//     return bytesRead;

//   // TODO:

//   // This function supports reading data from the sensor hub.
//   // It will be called frequently to service the device.
//   //
//   // If the HAL has received a full SHTP transfer, this function
//   // should load the data into pBuffer, set the timestamp to the
//   // time the interrupt was detected, and return the non-zero length
//   // of data in this transfer.
//   //
//   // If the HAL has not recevied a full SHTP transfer, this function
//   // should return 0.
//   //
//   // Because this function is called regularly, it can be used to
//   // perform other housekeeping operations.  (In the case of UART
//   // interfacing, bytes transmitted are staggered in time and this
//   // function can be used to keep the transmission flowing.)


//   // My notes:
//   // One thing I need to find out is how the interrupt line
//   // plays into recieving... if we even need it?
//   // TODO is to get this to return the number of bytes it read...
//     // readBytes(pBuffer, len);
//     // printf("xfer size: %d\n", i2c1.handle->XferSize);
//     // // static bool is_first_read = true;
//     // // if (is_first_read) {
//     // //   is_first_read = false;
//     // //   return 4;
//     // // }

//     // return 255 - (i2c1.handle->XferSize);
//     // return 2;
//   // }


//   // return 0;

//     // For blocking I2C, we need to check if data is available first
//     // The BNO085 uses a header byte to indicate data length
//     // uint8_t header[4];

//     // // Read the 4-byte SHTP header (blocking)
//     // if (HAL_I2C_Master_Receive(i2c1.handle, I2C_ADDR << 1, header, 4, 100) != HAL_OK) {
//     //     return 0; // No data available or error
//     // }

//     // // Parse SHTP header to get cargo length
//     // uint16_t cargoLength = ((uint16_t)header[0] + ((uint16_t)header[1] << 8)) & ~0x8000;

//     // if (cargoLength == 0) {
//     //     return 0; // No data
//     // }

//     // if (cargoLength > len) {
//     //     return 0; // Buffer too small
//     // }

//     // // Copy header to buffer
//     // memcpy(pBuffer, header, 4);

//     // // Read remaining cargo if any (blocking)
//     // if (cargoLength > 4) {
//     //     if (HAL_I2C_Master_Receive(i2c1.handle, I2C_ADDR << 1,
//     //                                pBuffer + 4, cargoLength - 4, 100) != HAL_OK) {
//     //         return 0;
//     //     }
//     // }

//     // // Get timestamp
//     // if (t_us) {
//     //     *t_us = getTimeUs(self);
//     // }
//     // printf("cargoLength: %d\n", cargoLength);
//     // // printf("cargo: ");
//     // // for (int j = 0; j < cargoLength; j++) {
//     // //   printf("0x%02" PRIx8 ",", pBuffer[j]);
//     // // }
//     // // printf("\n");
//     // return cargoLength;

// }

// int Bno085::write(sh2_Hal_t *self, uint8_t *pBuffer, unsigned len) {
//   (void) self;

//   // TODO:
//   // This function supports writing data to the sensor hub.
//   // It is called each time the application has a block of data to
//   // transfer to the device.
//   //
//   // If the device isn't ready to receive data, this function can
//   // return 0 without performing the transmit function.
//   //
//   // If the transmission can be started, this function needs to
//   // copy the data from pBuffer and return the number of bytes
//   // accepted.  It need not block.  The actual transmission of
//   // the data can continue after this function returns.

//   // return writeBytes(pBuffer, len);
//   // Blocking I2C write
//   if (HAL_I2C_Master_Transmit(i2c1.handle, I2C_ADDR << 1, pBuffer, len, 100) != HAL_OK) {
//       return 0;
//   }

//   return len;
// }

// uint32_t Bno085::getTimeUs(sh2_Hal_t *self) {
//   (void)self;
//   // This function should return a 32-bit value representing a
//   // microsecond counter.  The count may roll over after 2^32
//   // microseconds.

//   // My notes:
//   // Done!
//   return (uptimeGetMs() * 1000);
// }
