#include "bno085.h"
#include "uptime.h"
#include "debug.h"
#include "bsp.h"
#include "string.h"

Bno085::Bno085(I2CInterface_t* i2cInterface, uint8_t address) {
  _interface = i2cInterface;
  _addr = static_cast<uint8_t>(address);
}

bool Bno085::init() {
  return true;
}

int Bno085::open(sh2_Hal_t *self) {
  (void) self;
  // TODO - this is what needs to happen here:

  // This function initializes communications with the device.  It
  // can initialize any GPIO pins and peripheral devices used to
  // interface with the sensor hub.
  // It should also perform a reset cycle on the sensor hub to
  // ensure communications start from a known state.

  // My notes:
  // I believe since we always have the bus on we just need to
  // do the reset portion of above

  // I'm not sure what needs to be returned here for success/fail
  return 0;
}

void Bno085::close(sh2_Hal_t *self) {
  (void) self;
  // TODO:
  // This function completes communications with the sensor hub.
  // It should put the device in reset then de-initialize any
  // peripherals or hardware resources that were used.

  // My notes:
  // Since we are not actually turning off the HAL/I2C bus here
  // I think maybe we just use a private variable to block tx/rx
  // from the sensor
}

int Bno085::read(sh2_Hal_t *self, uint8_t *pBuffer, unsigned len, uint32_t *t_us) {
  (void) self;
  (void)t_us;

  // TODO:

  // This function supports reading data from the sensor hub.
  // It will be called frequently to service the device.
  //
  // If the HAL has received a full SHTP transfer, this function
  // should load the data into pBuffer, set the timestamp to the
  // time the interrupt was detected, and return the non-zero length
  // of data in this transfer.
  //
  // If the HAL has not recevied a full SHTP transfer, this function
  // should return 0.
  //
  // Because this function is called regularly, it can be used to
  // perform other housekeeping operations.  (In the case of UART
  // interfacing, bytes transmitted are staggered in time and this
  // function can be used to keep the transmission flowing.)


  // My notes:
  // One thing I need to find out is how the interrupt line
  // plays into recieving... if we even need it?
  // TODO is to get this to return the number of bytes it read...
    // readBytes(pBuffer, len);
    // printf("xfer size: %d\n", i2c1.handle->XferSize);
    // // static bool is_first_read = true;
    // // if (is_first_read) {
    // //   is_first_read = false;
    // //   return 4;
    // // }

    // return 255 - (i2c1.handle->XferSize);
    // return 2;
  // }


  // return 0;

    // For blocking I2C, we need to check if data is available first
    // The BNO085 uses a header byte to indicate data length
    uint8_t header[4];

    // Read the 4-byte SHTP header (blocking)
    if (HAL_I2C_Master_Receive(i2c1.handle, I2C_ADDR << 1, header, 4, 100) != HAL_OK) {
        return 0; // No data available or error
    }

    // Parse SHTP header to get cargo length
    uint16_t cargoLength = ((uint16_t)header[0] + ((uint16_t)header[1] << 8)) & ~0x8000;

    if (cargoLength == 0) {
        return 0; // No data
    }

    if (cargoLength > len) {
        return 0; // Buffer too small
    }

    // Copy header to buffer
    memcpy(pBuffer, header, 4);

    // Read remaining cargo if any (blocking)
    if (cargoLength > 4) {
        if (HAL_I2C_Master_Receive(i2c1.handle, I2C_ADDR << 1,
                                   pBuffer + 4, cargoLength - 4, 100) != HAL_OK) {
            return 0;
        }
    }

    // Get timestamp
    if (t_us) {
        *t_us = getTimeUs(self);
    }
    printf("cargoLength: %d\n", cargoLength);
    // printf("cargo: ");
    // for (int j = 0; j < cargoLength; j++) {
    //   printf("0x%02" PRIx8 ",", pBuffer[j]);
    // }
    // printf("\n");
    return cargoLength;

}

int Bno085::write(sh2_Hal_t *self, uint8_t *pBuffer, unsigned len) {
  (void) self;

  // TODO:
  // This function supports writing data to the sensor hub.
  // It is called each time the application has a block of data to
  // transfer to the device.
  //
  // If the device isn't ready to receive data, this function can
  // return 0 without performing the transmit function.
  //
  // If the transmission can be started, this function needs to
  // copy the data from pBuffer and return the number of bytes
  // accepted.  It need not block.  The actual transmission of
  // the data can continue after this function returns.

  // return writeBytes(pBuffer, len);
  // Blocking I2C write
  if (HAL_I2C_Master_Transmit(i2c1.handle, I2C_ADDR << 1, pBuffer, len, 100) != HAL_OK) {
      return 0;
  }

  return len;
}

uint32_t Bno085::getTimeUs(sh2_Hal_t *self) {
  (void)self;
  // This function should return a 32-bit value representing a
  // microsecond counter.  The count may roll over after 2^32
  // microseconds.

  // My notes:
  // Done!
  return (uptimeGetMs() * 1000);
}
