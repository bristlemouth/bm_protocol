//
// Created by Genton Mo on 1/7/21.
//

#pragma once

#include "abstract/abstract_i2c.h"
#include "debug.h"
#include "task.h"
#include "math.h"
#include "protected_i2c.h"
#include "abstract_htu_sensor.h"

#define HTU21D_ADDR 0x40

#define HTU21D_TRIGGER_TEMP_MEASURE_HOLD  0xE3
#define HTU21D_TRIGGER_HUMD_MEASURE_HOLD  0xE5
#define HTU21D_TRIGGER_TEMP_MEASURE_NOHOLD  0xF3
#define HTU21D_TRIGGER_HUMD_MEASURE_NOHOLD  0xF5
#define HTU21D_WRITE_USER_REG  0xE6
#define HTU21D_READ_USER_REG  0xE7
#define HTU21D_SOFT_RESET  0xFE

#define HTU21D_INVALID_TEMPERATURE 999
#define HTU21D_INVALID_HUMIDITY 998
#define HTU21D_MAX_INIT_ATTEMPTS 3

#define HTU21D_TEMPERATURE_MEASURING_TIME 51 // 50 ms + 1 ms for assurance
#define HTU21D_HUMIDITY_MEASURING_TIME 17 // 16 ms + 1 ms for assurance

#define HTU21D_CHECKSUM_DIVISOR 0x00988000 // Generator polynomial is X^8 + X^5 + X^4 + 1, shifted left to the 24th bit

class HTU21D : public AbstractI2C, public AbstractHtu {
public:
  HTU21D(I2CInterface_t* interface);

  bool init();
  bool read(float &temperature, float &humidity);
  float readTemperature();
  float readHumidity();

private:
  bool verifyChecksum(uint8_t * rxBuff);
  float _latestTemperature;
  float _latestHumidity;
};
