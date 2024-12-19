#include "tca9546a.h"
#include "debug.h"
#include "app_util.h"

using namespace TCA;

TCA9546A::TCA9546A(I2CInterface_t *interface, uint8_t address,
                   IOPinHandle_t *resetPin) {
  _interface = interface;
  _addr = static_cast<uint8_t>(address);
  _resetPin = resetPin;
}

/*!
 Initialize TCA9546A device. Try to set to CH_NONE.
 If this fails, try again (3 times), otherwise success

 /return true is successfull, false otherwise
*/
bool TCA9546A::init() {
  bool rval = false;
  printf("TCA9546A init\n");

  uint8_t retriesRemaining = 3;
  while (!rval && retriesRemaining--) {
    bool res = setChannel(CH_NONE);
    if (!res) {
      printf("Init failed retry - %u\n", retriesRemaining);
      continue;
    } else {
      printf("TCA9546A initialized!\n");
      rval = true;
      break;
    }
  };
  return rval;
}

/*!
 Set the 8-bit (only uses the lower 4 bits) control register
 to select a combination of channels to enable.

 Example: setChannel(TCA::CH_1 | TCA::CH_2) will enable channels 1 and 2

 \param[in] channels Channels to enable
 \return true if successfull, false otherwise
*/
bool TCA9546A::setChannel(Channel_t channels) {
  bool rval = false;
  do {
    if (channels == CH_UNKNOWN) {
      break;
    }
    if (writeBytes(&channels, sizeof(uint8_t)) != I2C_OK) {
      break;
    }
    Channel_t temp_channel = CH_UNKNOWN;
    if (!getChannel(temp_channel)) {
      break;
    }
    if (temp_channel != channels) {
      break;
    }
    rval = true;
  } while (0);

  return rval;
}

/*!
  Read the 8-bit control register to get the set of currently enabled channels

  \return true if successfull, false otherwise
*/
bool TCA9546A::getChannel(Channel_t &channel) {
  bool rval = false;
  if (readBytes(&channel, sizeof(uint8_t), 100) == I2C_OK) {
    rval = true;
  }

  return rval;
}

/*!
 Reset the i2c mux. This will clear the channel selection, leaving no channels selected.
*/
void TCA9546A::hwReset() {
  IOWrite(_resetPin, 0);
  vTaskDelay(pdMS_TO_TICKS(50));
  IOWrite(_resetPin, 1);
  vTaskDelay(pdMS_TO_TICKS(10));
}
