#include "debug.h"
#include "tca9546a.h"
#include "util.h"

using namespace TCA;

TCA9546A::TCA9546A(I2CInterface_t* interface, uint8_t address, IOPinHandle_t *resetPin)
{
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
  while(!rval && retriesRemaining--){
    bool res = setChannel(CH_NONE);
    if(!res){
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
 Set the 8-bit(only uses the lower 4 bits) control register to select a channel

 \param[in] channel Channel to select
 \return true if successfull, false otherwise
*/
bool TCA9546A::setChannel(Channel_t channel) {
  bool rval = false;
  do {
    if (channel == CH_UNKNOWN) {
      break;
    }
    if (writeBytes((uint8_t*)&channel, sizeof(uint8_t)) != I2C_OK) {
      break;
    }
    Channel_t temp_channel = CH_UNKNOWN;
    if (!getChannel(temp_channel)) {
      break;
    }
    if(temp_channel != channel) {
      break;
    }
    rval = true;
  } while (0);

  return rval;
}

/*!
  Read the 8-bit control register to get the current channel

  \return true if successfull, false otherwise
*/
bool TCA9546A::getChannel(Channel_t &channel){
  bool rval = false;
  if (readBytes((uint8_t*)&channel, sizeof(uint8_t), 100) == I2C_OK){
    rval = true;
  }

  return rval;
}

/*!
 Reset the i2c mux. This will clear the channel selection, leaving no channels selected.
*/
void TCA9546A::hwReset(){
  IOWrite(_resetPin, 0);
  vTaskDelay(pdMS_TO_TICKS(50));
  IOWrite(_resetPin, 1);
  vTaskDelay(pdMS_TO_TICKS(10));
}
