#include "debug.h"
#include "tca9546a.h"
#include "util.h"

using namespace TCA;

TCA9546A::TCA9546A(I2CInterface_t* interface, uint8_t address, IOPinHandle_t *resetPin)
{
  _interface = interface;
  _addr = static_cast<uint8_t>(address);
  _resetPin = resetPin;

  // A Power on Reset will reset the channel to CH_NONE
  _channel = CH_NONE;
}

/*!
 Initialize TCA9546A device. Check that it is present and read the default
 control register, which should be 0x00

 /return true is successfull, false otherwise
*/
bool TCA9546A::init() {
  bool rval = false;
  printf("TCA9546A init\n");

  uint8_t retriesRemaining = 3;
  while(!rval && retriesRemaining--){
    Channel_t chnl;
    getChannel(&chnl);
    if(chnl != _channel){
      printf("Invalid channel");
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
  if(channel != _channel){
    return (writeBytes((uint8_t*)&channel, sizeof(uint8_t)) == I2C_OK);
  } else {
    return true;
  }
}

/*!
  Read the 8-bit control register to get the current channel

  \return true if successfull, false otherwise
*/
bool TCA9546A::getChannel(Channel_t *channel){
  bool rval = false;
  if (readBytes((uint8_t*)channel, sizeof(uint8_t), 100) == I2C_OK){
    _channel = *channel;
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