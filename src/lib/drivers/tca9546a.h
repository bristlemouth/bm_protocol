#pragma once

#include "abstract/abstract_i2c.h"
#include "io.h"

namespace TCA {

typedef enum {
  CH_NONE = 0x00,
  CH_1 = 0x01,
  CH_2 = 0x02,
  CH_3 = 0x04,
  CH_4 = 0x08,
} Channel_t;

class TCA9546A : public AbstractI2C {
public:
  TCA9546A(I2CInterface_t* interface, uint8_t address, IOPinHandle_t *resetPin);
  bool init();

  bool setChannel(Channel_t channel);
  bool getChannel(Channel_t *channel);
  void hwReset();

private:
  Channel_t _channel;
  IOPinHandle_t *_resetPin;
};

} // namespace TCA