#pragma once

#include "abstract/abstract_i2c.h"

namespace BQ {

constexpr uint8_t I2C_DEFAULT_ADDR = 0x6B;

typedef enum {
    POWER_PATH_REG = 0x19,
    PART_INFO_REG = 0x3D,
} Reg_t

class BQ25820 : public AbstractI2C {
public:
  BQ26820(I2CInterface_t* interface, uint8_t address=I2C_DEFAULT_ADDR);
  bool init();
  bool disablePfm();

private:
  bool setCfgBits(uint16_t bits, uint8_t mask, uint8_t shift);
  bool readReg(Reg_t reg, uint16_t *val);
  bool read8(Reg_t reg, uint8_t *val);
  bool writeReg(Reg_t reg, uint16_t val);
  bool write8(Reg_t reg, uint8_t val);
  int decodeTwosComplBits(uint16_t bits, uint16_t mask);

  float _shunt;
  float _voltage;
  float _current;
  ConvTime_t _busCT;
  ConvTime_t _shuntCT;
  Avg_t _avg;

  uint16_t _cfg;
};

} // namespace INA232
