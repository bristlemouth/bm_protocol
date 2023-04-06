#include "FreeRTOS.h"
#include "task.h"
#include "ms5803.h"
#include "debug.h"

MS5803::MS5803(I2CInterface_t* i2cInterface, uint8_t address) {
  _interface = i2cInterface;
  _addr = static_cast<uint8_t>(address);
}

/*!
  Initialize MS5803. Resets and reads PROM

  \return Will return false if sensor is not present or communicating
*/
bool MS5803::init() {
  bool rval = true;

  rval &= reset();
  rval &= readPROM();

  return rval;
}

/*!
  Reset to make sure calibration PROM gets loaded into register.

  \return true if successfull false otherwise
*/
bool MS5803::reset() {
  bool rval = sendCommand(CMD_RESET, 10);
  vTaskDelay(pdMS_TO_TICKS(10));
  return rval;
}

/*!
  Read compensated pressure and temperature from device.

  \param[out] *pressure pointer to variable in which to store pressure in mbar
  \param[out] *temperature pointer to variable in which to store temperature in C
  \return true if successfull false otherwise
*/
bool MS5803::readPTRaw(float &pressure, float &temperature) {

  bool rval = true;

  do {
    uint32_t rawTemperature = 0;
    uint32_t rawPressure = 0;

    // 10ms conversion delay for 4096
    rval = getRawValue(CMD_CONV_D2_4096, &rawTemperature, 10, 10);
    if (!rval) {
      break;
    }

    // 10ms conversion delay for 4096
    rval = getRawValue(CMD_CONV_D1_4096, &rawPressure, 10, 10);
    if (!rval) {
      break;
    }

    int32_t dT = rawTemperature - _PROM.T_REF * (1UL << 8);
    int64_t off = (int64_t)_PROM.OFF * (1UL << 17) + ((int64_t)dT * (int64_t)_PROM.TCO)/(1UL << 6);
    int64_t sens = (int64_t)_PROM.SENS * (1UL << 16) + ((int64_t)dT * (int64_t)_PROM.TCS)/(1UL << 7);

    int32_t _temperature = (2000 + ((int64_t)dT * (int64_t)_PROM.TEMPSENS)/(1UL << 23));

    // Second order temperature compensation
    int64_t t2 = 0;
    int64_t off2 = 0;
    int32_t sens2 = 0;

    if(_temperature < 2000) {
      t2 = ((int64_t)dT * (int64_t)dT) / (1ULL << 31);
      off2 = (61 * (_temperature - 2000) * (_temperature - 2000))/(1 << 4);
      sens2 = (2 * ((_temperature - 2000) * (_temperature - 2000)));
    }

    if(_temperature < -1500) {
      off2 = off2 + 20 * ((_temperature + 1500) * (_temperature + 1500));
      sens2 = sens2 + 12 * ((_temperature + 1500) * (_temperature + 1500));
    }

    _temperature -= t2;
    off -= off2;
    sens -= sens2;

    int32_t _pressure = (((rawPressure * sens/(1UL << 21)) - off)/(1UL << 15));

    // Check for valid temperature
    if((_temperature < -4000) || (_temperature > 8500)) {
      rval = false;
      break;
    }

    // Check for valid pressure
    if((_pressure < 1000) || (_pressure > 120000)) {
      rval = false;
      break;
    }

    pressure = (float)_pressure/100.0;
    temperature = (float)_temperature/100.0;

  } while (0);

  return rval;
}

/*!
  Read raw pressure or temperature from device

  \param[in] command Command to start measurement (for D1 or D2)
  \param[out] *value pointer to variable in which to store raw value
  \param[in] timeoutMs SPI transaction timeout in milliseconds
  \param[in] conversion delay in milliseconds
  \return true if successfull false otherwise
*/
bool MS5803::getRawValue(MS5803Cmd_t command, uint32_t *value, uint32_t timeoutMs, uint32_t delayMs) {
  bool rval = true;
  configASSERT(value != NULL);

  do {
    rval = sendCommand(command, timeoutMs);
    if(!rval) {
      break;
    }

    vTaskDelay(pdMS_TO_TICKS(delayMs)); // 10ms delay for conversion

    uint8_t adc[3] = {0,0,0};
    rval = readData(CMD_ADC_READ, adc, sizeof(adc));
    if(!rval) {
      break;
    }

    *value = ((uint32_t)adc[0] << 16) + ((uint32_t)adc[1] << 8) + (uint32_t)adc[2];
  } while (0);

  return rval;
}

/*!
  Check if PROM CRC is valid

  \return true if successfull false otherwise

  CRC algorithm derived from AN520
  https://www.te.com.cn/commerce/DocumentDelivery/DDEController?Action=showdoc&DocId=Specification+Or+Standard%7FAN520_C-code_example_for_MS56xx%7FA%7Fpdf%7FEnglish%7FENG_SS_AN520_C-code_example_for_MS56xx_A.pdf%7FCAT-BLPS0003
*/
bool MS5803::crc4Valid() {

  uint32_t nRem = 0;
  uint16_t crcBackup = _PROM.c[7];

  _PROM.c[7] &= 0xFF00;

  for(uint16_t cnt = 0; cnt < 16; cnt++) {

    if(cnt % 2 == 1) {
      nRem ^= (uint16_t)(_PROM.c[cnt >> 1] & 0xFF);
    } else {
      nRem ^= (uint16_t)(_PROM.c[cnt >> 1]  >> 8);
    }

    for(uint8_t nBit = 8; nBit > 0; nBit--) {
      if(nRem & 0x8000) {
        nRem = (nRem << 1) ^ 0x3000;
      } else {
        nRem = (nRem << 1);
      }
    }

  }

  nRem = (0xF & (nRem >> 12));
  _PROM.c[7] = crcBackup;

  return (nRem ^ 0x00) == (_PROM.c[7] & 0xF);
}

/*!
  Read device prom with calibration parameters

  \return true if successfull false otherwise
*/
bool MS5803::readPROM() {
  bool rval;

  for(uint8_t addr = 0; addr <= (uint8_t)(CMD_PROM_E-CMD_PROM_0)/2; addr += 1) {
    rval = readData((MS5803Cmd_t)(addr * 2 + (uint8_t)CMD_PROM_0),
                      (uint8_t *)&_PROM.c[addr], sizeof(uint16_t));
    _PROM.c[addr] = __builtin_bswap16(_PROM.c[addr]);
    if(!rval) {
      break;
    }
  }

  // Check to see if the prom is all zeroes, since the CRC will not catch
  // that as a problem
  uint16_t promBits = 0;
  for(uint8_t idx = 0; idx < sizeof(MS5803Prom_t)/sizeof(uint16_t); idx++) {
    promBits |= _PROM.c[idx];
  }

  if (promBits == 0) {
    rval = false;
  }

  if(rval) {
    if(!crc4Valid()) {
      rval = false;
      printf("MS5803 - Invalid CRC\n");
    }
  }

  return rval;
}

/*!
  Use PROM coefficients to get 32-bit unique-ish signature for device identification
  We're assuming that the coefficients are unique and static for each device

  \return return "unique" sensor signature
*/
uint32_t MS5803::signature() {
  uint32_t sig = 0;
  uint32_t *promPtr = (uint32_t *)_PROM.c;
  for(uint8_t byte=0; byte<4; byte++) {
    sig ^= promPtr[byte];
  }
  return sig;
}

/*!
  Make sure reading prom works.
  This can be run anytime to check for SPI bus corruption. (If we get random
  data back from SPI bus, it could show up as valid temp/pressure, but the
  PROM checksum will most likely fail to validate.)

  \return true if successfull false otherwise
*/
bool MS5803::checkPROM() {
  bool rval = readPROM();

  return rval;
}

/*!
  Send command to device

  \param[out] command Command to send
  \param[out] timeoutMs SPI transaction timeout

  \return true if successfull false otherwise
*/
bool MS5803::sendCommand( MS5803Cmd_t command, uint32_t timeoutMs ){
  (void) timeoutMs;
  return (writeBytes((uint8_t*)&command, sizeof(uint8_t)) == I2C_OK);
}

/*!
  Read bytes after sending a command

  \param[out] *command command/address to read data from
  \param[out] *data pointer to buffer in which to store data in
  \param[out] *dataSize size of data buffer
  \return true if successfull false otherwise
*/
bool MS5803::readData(MS5803Cmd_t command, uint8_t *data, size_t dataSize) {
  I2CResponse_t rval;

  do {
    rval = writeBytes((uint8_t*)&command, sizeof(uint8_t));
    if(rval != I2C_OK){
      printf("error writing to MS5803: %d\n", rval);
      break;
    }

    rval = readBytes(data, dataSize);
    if(rval != I2C_OK){
      printf("error reading bytes from MS5803: %d\n", rval);
    }

  } while (0);

  return (rval == I2C_OK);
}
