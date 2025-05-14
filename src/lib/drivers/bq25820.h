#pragma once

#include "abstract/abstract_i2c.h"

namespace BQ {

constexpr uint8_t I2C_DEFAULT_ADDR = 0x6B;

typedef enum {
    POWER_PATH_REG = 0x19,
    PART_INFO_REG = 0x3D,
    ADC_CTRL_REG = 0x2B,
    CHARGER_FLAG_1 = 0x25,
    IAC_ADC_REG = 0x2D,
    IBAT_ADC_REG = 0x2F,
    VAC_ADC_REG = 0x31,
    VBAT_ADC_REG = 0x33,
    VSYS_ADC_REG = 0x35,
    TS_ADC_REG = 0x37

} Reg_t;

class BQ25820 : public AbstractI2C {
public:
  BQ25820(I2CInterface_t* interface, uint8_t address=I2C_DEFAULT_ADDR);
  bool init();
  bool disablePfm();
  bool readSensors();

private:
  bool setCfgBits(uint16_t bits, uint8_t mask, uint8_t shift);
  bool readReg(Reg_t reg, uint16_t *value);
  bool read8(Reg_t reg, uint8_t *value);
  bool writeReg(Reg_t reg, uint16_t value);
  bool write8(Reg_t reg, uint8_t value);
  int decodeTwosComplBits(uint16_t bits, uint16_t mask);
};

} // namespace BQ
