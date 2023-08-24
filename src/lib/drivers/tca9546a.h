#pragma once

#include "abstract/abstract_i2c.h"
#include "io.h"

namespace TCA {

typedef uint8_t Channel_t;
constexpr Channel_t CH_NONE = 0x00;
constexpr Channel_t CH_1 = 0x01;
constexpr Channel_t CH_2 = 0x02;
constexpr Channel_t CH_3 = 0x04;
constexpr Channel_t CH_4 = 0x08;
constexpr Channel_t CH_UNKNOWN = 0x10;

class TCA9546A : public AbstractI2C {
public:
  TCA9546A(I2CInterface_t *interface, uint8_t address, IOPinHandle_t *resetPin);
  bool init();

  bool setChannel(Channel_t channel);
  bool getChannel(Channel_t &channel);
  void hwReset();

private:
  IOPinHandle_t *_resetPin;
};

} // namespace TCA
