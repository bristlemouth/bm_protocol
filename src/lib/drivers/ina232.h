#pragma once

#include "abstract/abstract_i2c.h"

namespace INA {

constexpr uint8_t I2C_DEFAULT_ADDR = 0x41;

typedef enum {
  REG_CFG = 0,
  REG_SHUNT_V       = 0x01, // Averaged shunt voltage value
  REG_BUS_V         = 0x02, // Averaged bus voltage value
  PWR_REG           = 0x03, // Averaged Power Register
  CUR_REG           = 0x04, // Averaged Current register
  CAL_REG           = 0x05, // Sets value of calibration shunt resistor
  REG_MASK_EN       = 0x06, // Mask/Enable
  REG_CA_LIM        = 0x07, // Critical Alert Limit
  REG_MFG_ID        = 0x3E, // Manufacturer ID
} Reg_t;

typedef enum{
  AVG_1 = 0x0,
  AVG_4,
  AVG_16,
  AVG_64,
  AVG_128,
  AVG_256,
  AVG_512,
  AVG_1024
} Avg_t;

typedef enum{
  CT_140 = 0x0,
  CT_203,
  CT_332,
  CT_588,
  CT_1100,
  CT_2116,
  CT_4156,
  CT_8244
} ConvTime_t;

class INA232 : public AbstractI2C {
public:
  INA232(I2CInterface_t* interface, uint8_t address=I2C_DEFAULT_ADDR);
  bool init();

  bool setShuntValue(float resistance);
  bool setAvg(Avg_t avg);
  bool setBusConvTime(ConvTime_t convTime);
  bool setShuntConvTime(ConvTime_t convTime);
  bool measurePower();
  void getPower(float &voltage, float &current);
  uint32_t getTotalConversionTimeMs();
  uint16_t getAddr();

private:
  bool setCfgBits(uint16_t bits, uint8_t mask, uint8_t shift);
  bool readReg(Reg_t reg, uint16_t *val);
  bool writeReg(Reg_t reg, uint16_t val);
  bool waitForReadyFlag();
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
