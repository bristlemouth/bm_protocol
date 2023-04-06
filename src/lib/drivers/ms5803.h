#pragma once

#include "stm32u5xx_hal.h"
#include "abstract_i2c.h"
#include "io.h"

// Using the MS5803_02BA

typedef enum {
  CMD_RESET         = 0x1E,
  CMD_CONV_D1_256   = 0x40,
  CMD_CONV_D1_512   = 0x42,
  CMD_CONV_D1_1024  = 0x44,
  CMD_CONV_D1_2048  = 0x46,
  CMD_CONV_D1_4096  = 0x48,
  CMD_CONV_D2_256   = 0x50,
  CMD_CONV_D2_512   = 0x52,
  CMD_CONV_D2_1024  = 0x54,
  CMD_CONV_D2_2048  = 0x56,
  CMD_CONV_D2_4096  = 0x58,
  CMD_ADC_READ      = 0x00,
  CMD_PROM_0        = 0xA0,
  CMD_PROM_1        = 0xA1,
  CMD_PROM_2        = 0xA2,
  CMD_PROM_3        = 0xA3,
  CMD_PROM_4        = 0xA4,
  CMD_PROM_5        = 0xA5,
  CMD_PROM_6        = 0xA6,
  CMD_PROM_7        = 0xA7,
  CMD_PROM_8        = 0xA8,
  CMD_PROM_9        = 0xA9,
  CMD_PROM_A        = 0xAA,
  CMD_PROM_B        = 0xAB,
  CMD_PROM_C        = 0xAC,
  CMD_PROM_D        = 0xAD,
  CMD_PROM_E        = 0xAE,
} MS5803Cmd_t;

typedef union {
  uint16_t c[8];
  struct {
    uint16_t RSVD;      // Reserved
    uint16_t SENS;      // Pressure sensitvity
    uint16_t OFF;       // Pressure offset
    uint16_t TCS;       // Temperature coefficient of pressure sensitivity
    uint16_t TCO;       // Temperature coefficient of pressure offset
    uint16_t T_REF;     // Reference Temperature
    uint16_t TEMPSENS;  // Temperature coefficient of the temperature
    uint16_t CRC4;      // CRC4
  };
} MS5803Prom_t;

class MS5803 : public AbstractI2C {
public:
  MS5803(I2CInterface_t* i2cInterface, uint8_t address);

  bool init();
  bool reset();
  bool readPTRaw(float &pressure, float &temperature);
  bool checkPROM();
  uint32_t signature();

private:
  bool readPROM();
  bool sendCommand(MS5803Cmd_t command, uint32_t timeoutMs);
  bool readData(MS5803Cmd_t command, uint8_t *data, size_t dataSize);
  bool crc4Valid();
  bool getRawValue(MS5803Cmd_t command, uint32_t *value, uint32_t timeoutMs, uint32_t delayMs);

  MS5803Prom_t _PROM;
};
