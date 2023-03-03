//
// Created by Genton Mo on 1/7/21.
//

#include "htu21d.h"

HTU21D::HTU21D(I2CInterface_t * interface):
  _latestTemperature(HTU21D_INVALID_TEMPERATURE),
  _latestHumidity(HTU21D_INVALID_HUMIDITY)
{
  _interface = interface;
  _addr = ((uint8_t) HTU21D_ADDR);
}

bool HTU21D::init() {
  uint8_t initAttempts = 0;

  while ((_latestTemperature == HTU21D_INVALID_TEMPERATURE || _latestHumidity == HTU21D_INVALID_HUMIDITY) && initAttempts++ < HTU21D_MAX_INIT_ATTEMPTS) {
    readHumidity();
    readTemperature();
  }

  bool rval = (initAttempts <= HTU21D_MAX_INIT_ATTEMPTS);

  return rval;
}

bool HTU21D::read(float &temperature, float &humidity) {
  float tmpTemperature = readTemperature();
  float tmpHumidity = readHumidity();
  bool rval = false;

  if((tmpTemperature != HTU21D_INVALID_TEMPERATURE) &&
      (tmpHumidity != HTU21D_INVALID_HUMIDITY)) {
    rval = true;
    temperature = tmpTemperature;
    humidity = tmpHumidity;
  }

  return rval;
}

float HTU21D::readTemperature() {
  uint8_t txData[] = { HTU21D_TRIGGER_TEMP_MEASURE_NOHOLD };
  uint8_t rxData[3];

  I2CResponse_t rval = writeBytes(txData, sizeof(txData), 100);
  vTaskDelay(pdMS_TO_TICKS(HTU21D_TEMPERATURE_MEASURING_TIME));
  if (rval == I2C_OK) rval = readBytes(rxData, sizeof(rxData), 100);
  if (rval == I2C_OK) {
    if (verifyChecksum(rxData)) {
      uint16_t temperatureBits = rxData[0] << 8 | rxData[1];
      temperatureBits &= 0xFFFC; // zero out status bits
      _latestTemperature = -46.85 + 175.72 * ((float)temperatureBits) / pow(2, 16);
    }
    else {
      printf("HTU21D readTemperature invalid checksum!");
      _latestTemperature = HTU21D_INVALID_TEMPERATURE;
    }
  }
  else if (rval == I2C_MUTEX) printf("HTU21D readTemperature i2c bus busy!");
  else _latestTemperature = HTU21D_INVALID_TEMPERATURE;
  return _latestTemperature;
}

float HTU21D::readHumidity() {
  uint8_t txData[] = { HTU21D_TRIGGER_HUMD_MEASURE_NOHOLD };
  uint8_t rxData[3];

  I2CResponse_t rval = writeBytes(txData, sizeof(txData), 100);
  vTaskDelay(pdMS_TO_TICKS(HTU21D_HUMIDITY_MEASURING_TIME));
  if (rval == I2C_OK) rval = readBytes(rxData, sizeof(rxData), 100);
  if (rval == I2C_OK) {
    if (verifyChecksum(rxData)) {
      uint16_t humidityBits = rxData[0] << 8 | rxData[1];
      humidityBits &= 0xFFF0; // zero out status bits
      _latestHumidity = -6 + 125 * ((float)humidityBits) / pow(2, 16);
    }
    else {
      printf("HTU21D readHumidity invalid checksum!");
      _latestHumidity = HTU21D_INVALID_HUMIDITY;
    }
  }
  else if (rval == I2C_MUTEX) printf("HTU21D readHumidity i2c bus busy!");
  else _latestHumidity = HTU21D_INVALID_HUMIDITY;
  return _latestHumidity;
}

// rxBuff always has a length of 3: 2 data bytes + 1 crc byte
// Data is valid if the remainder from checksum algo == crc byte
bool HTU21D::verifyChecksum(uint8_t * rxBuff) {
  uint32_t remainder = rxBuff[0] << 16 | rxBuff[1] << 8; // Pad n 0s to LSB (where n = number of checksum bits)
  uint32_t divisor = HTU21D_CHECKSUM_DIVISOR;
  uint32_t mask = 1 << 23;

  for (uint8_t i = 0 ; i < 16 ; i++) { // Operate on only 16 positions of max 24. The remaining 8 are our remainder and should be zero when we're done.
    if (remainder & mask) remainder ^= divisor; // Check if there is a one in the left position
    mask >>= 1;
    divisor >>= 1; // Rotate the divisor max 16 times so that we have 8 bits left of a remainder
  }
  return remainder == rxBuff[2];
}
