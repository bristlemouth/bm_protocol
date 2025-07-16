#pragma once

#include "abstract/abstract_i2c.h"

namespace BQ {

constexpr uint8_t I2C_DEFAULT_ADDR = 0x6B;

typedef enum {
    CHARGE_VLIM_REG = 0x00,
    CC_LIM_REG = 0x02,
    TIMER_CTRL_REG = 0x15,
    CHARGER_CTRL_REG = 0x17,
    POWER_PATH_REG = 0x19,
    PART_INFO_REG = 0x3D,
    FAULT_FLAG_REG = 0x27,
    ADC_CTRL_REG = 0x2B,
    ADC_CH_CTRL_REG = 0x2C,
    CHARGER_FLAG_1 = 0x25,
    
    IAC_ADC_REG = 0x2D,
    IBAT_ADC_REG = 0x2F,
    VAC_ADC_REG = 0x31,
    VBAT_ADC_REG = 0x33,
    VSYS_ADC_REG = 0x35,
    TS_ADC_REG = 0x37,
    VFB_ADC_REG = 0x39

} Reg_t;

class BQ25820 : public AbstractI2C {
public:
  BQ25820(I2CInterface_t* interface, uint8_t address=I2C_DEFAULT_ADDR);
  bool init();
  bool disablePfm();
  bool readSensors(char *buffer, size_t len);
  bool readFaults(char *buffer, size_t len);

private:
  bool setCfgBits(uint16_t bits, uint8_t mask, uint8_t shift);
  bool read16(Reg_t reg, int16_t *value);
  bool read8(Reg_t reg, uint8_t *value);
  bool write16(Reg_t reg, uint16_t value);
  bool write8(Reg_t reg, uint8_t value);
  int decodeTwosComplBits(uint16_t bits, uint16_t mask);
};

} // namespace BQ
