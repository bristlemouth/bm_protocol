#include "ina232.h"
#include "debug.h"
#include "app_util.h"

using namespace INA;

/// INA232 manufactureer id register
#define MFG_ID 0x5449

/// Conversion ready flag in mask/enable register
#define INA_CVRF (1 << 3)

/// Reset bit in configuration register
#define INA_RST (1 << 15)

/// Bit position for bus conversion time in configuration register
#define CFG_SHUNT_CT_POS (3)

/// Bit position for shunt conversion time in configuration register
#define CFG_BUS_CT_POS (6)

/// Bit position for averaging mode in configuration register
#define CFG_AVG_POS (9)

/// Milliseconds to wait between conversion ready flag checks
#define CVRF_POLL_PERIOD pdMS_TO_TICKS(50)

// Shunt voltage resolution in Volts
#define SHUNT_V_RESOLUTION_V (2.5e-6)

// Bus voltage resolution in Volts
#define BUS_V_RESOLUTION_V (1.6e-3)

#define CFG_BIT_MASK (0x7)

static const uint16_t inaAvgCounts[] = {1, 4, 16, 64, 128, 256, 512, 1024};
static const uint16_t inaConvTimes[] = {140, 203, 332, 588, 1100, 2116, 4156, 8244};

INA232::INA232(I2CInterface_t * interface, uint8_t address)
{
  _interface = interface;
  _addr = static_cast<uint8_t>(address);

  _shunt = 1e99;

  _busCT = CT_1100;
  _shuntCT = CT_1100;
  _avg = AVG_1;
}

/*!
  Initialize INA232 device. Check that it is present and set default configuration.

  \return true if successfull false otherwise
*/
bool INA232::init() {
  bool rval = false;
  uint16_t reg = 0;
  printf("INA232 Init\n");

  uint8_t retriesRemaining = 3;
  while (!rval && retriesRemaining--) {
    printf("Retries remaining %u\n", retriesRemaining);
    if(!readReg(REG_MFG_ID, &reg)) {
      continue;
    }
    printf("MFG_ID: %04X\n", reg);

    if(reg != MFG_ID) {
      printf("Invalid manufacturer id\n");
      continue;
    }

    // Reset to defaults
    reg = INA_RST;
    if(!writeReg(REG_CFG, reg)) {
      continue;
    }

    if(!readReg(REG_CFG, &reg)) {
      continue;
    }


    rval = true;
  };

  return rval;
}

/*!
  Read 16-bit register from device

  \param[in] reg Register address
  \param[out] *value register value
  \return true if successfull false otherwise
*/
bool INA232::readReg(Reg_t reg, uint16_t *value) {
  bool rval = false;
  uint8_t regByte = reg;
  uint16_t regVal = 0;

  I2CResponse_t res;
  do {
    res = writeBytes((uint8_t *)&regByte, sizeof(regByte), 100);
    if(res != I2C_OK) {
      printf("error writing bytes: %d\n", res);
      break;
    }
    res = readBytes((uint8_t *)&regVal, sizeof(regVal), 100);
    if(res != I2C_OK) {
      printf("error reading bytes: %d\n", res);
      break;
    }

    if(value != NULL) {
      *value = __builtin_bswap16(regVal);
    }

    rval = true;
  } while (0);

  return rval;
}

/*!
  Write 16-bit register to device

  \param[in] reg Register address
  \param[in] value Register value to set
  \return true if successfull false otherwise
*/
bool INA232::writeReg(Reg_t reg, uint16_t value) {
  uint8_t bytes[] = {static_cast<uint8_t>(reg), static_cast<uint8_t>(value >> 8), static_cast<uint8_t>(value & 0xFF)};
  return (writeBytes(bytes, sizeof(bytes), 100) == I2C_OK);
}

/*!
  Convert a u16-bit two's complement to a signed int

  \param[in] bits two's complement u16 to be converted
  \param[in] mask bitmask
  \return signed integer value of the two's complement
*/
int INA232::decodeTwosComplBits(uint16_t bits, uint16_t mask){
  int ret;
  if (bits & (~(mask >> 1) & mask)) {      //Check sign bit (MSB of mask)
    //Negative
    ret = -(((~bits) + 1) & (mask >> 1));   //2's complement
  } else {
    //Positive
    ret = bits & (mask >> 1);
  }
  return ret;
}


/*!
  Compute total conversion time (including averaging) in milliseconds

  \return number of milliseconds a conversion takes
*/
uint32_t INA232::getTotalConversionTimeMs() {
  return (((inaConvTimes[_shuntCT] + inaConvTimes[_busCT])) * inaAvgCounts[_avg])/1000;
}

/*!
  Set shunt resistor value

  \param[in] resistance Shunt resistance in ohms
  \return true if successfull false otherwise
*/
bool INA232::setShuntValue(float resistance) {
  // TODO - check for valid values
  _shunt = resistance;

  return true;
}

/*!
  Set bits in configuration register

  \param[in] bits Value to set in config
  \param[in] mask Mask for value size (since it will have to clear the bits first)
  \param[in] position Number of bits to left shift before clearing/setting value
  \return true if successfull false otherwise
*/
bool INA232::setCfgBits(uint16_t bits, uint8_t mask, uint8_t position) {
  bool rval = false;

  do {

    if(!readReg(REG_CFG, &_cfg)) {
      break;
    }

    // Clear current bits
    _cfg &= ~(mask << position);

    // Set new bits
    _cfg |= (static_cast<uint16_t>(bits) << position);

    if(!writeReg(REG_CFG, _cfg)) {
      break;
    }

    rval = true;
  } while (0);

  return rval;

}
/*!
  Set averaging configuration for all channels

  \param[in] avg Averaging configuration setting
  \return true if successfull false otherwise
*/
bool INA232::setAvg(Avg_t avg) {
  _avg = avg;
  return setCfgBits(static_cast<uint16_t>(avg), CFG_BIT_MASK, CFG_AVG_POS);
}

/*!
  Set bus voltage conversion time for all channels

  \param[in] convTime Conversion time setting
  \return true if successfull false otherwise
*/
bool INA232::setBusConvTime(ConvTime_t convTime) {
  _busCT = convTime;
  return setCfgBits(static_cast<uint16_t>(convTime), CFG_BIT_MASK, CFG_BUS_CT_POS);
}

/*!
  Set shunt voltage conversion time for all channels

  \param[in] convTime Conversion time setting
  \return true if successfull false otherwise
*/
bool INA232::setShuntConvTime(ConvTime_t convTime) {
  _shuntCT = convTime;
  return setCfgBits(static_cast<uint16_t>(convTime), CFG_BIT_MASK, CFG_SHUNT_CT_POS);
}

/*!
  Poll conversion ready flag

  \return true if conversion is ready, false otherwise
*/
bool INA232::waitForReadyFlag() {
  bool rval = false;
  uint32_t startTime = xTaskGetTickCount();
  uint32_t timeoutMs = getTotalConversionTimeMs();

  do {
    uint16_t regVal = 0;
    if(readReg((REG_MASK_EN), &regVal) && (regVal & INA_CVRF)) {
      rval = true;
      break;
    }

    vTaskDelay(CVRF_POLL_PERIOD);
  } while(timeRemainingTicks(startTime, pdMS_TO_TICKS(timeoutMs)));

  return rval;
}

/*!
  Get voltage/current measurements from all channels. To be later read with getChPower

  \return true if successfull false otherwise
*/
bool INA232::measurePower() {
  bool rval = true;
  uint16_t regVal;
  float shuntV, busV;

  do {

    if(!waitForReadyFlag()) {
      printf("Timed out waiting for ready flag!\n");
      rval = false;
      break;
    }

    // Read shunt voltage
    uint8_t reg =  static_cast<uint8_t>(REG_SHUNT_V);
    if(!readReg((Reg_t)reg, &regVal)) {
      rval = false;
      break;
    }

    shuntV = static_cast<float>(decodeTwosComplBits(static_cast<int16_t>(regVal), 0xFFFF)) * SHUNT_V_RESOLUTION_V;

    // Read bus voltage
    reg += 1;
    if(!readReg((Reg_t)reg, &regVal)) {
      rval = false;
      break;
    }

    busV = static_cast<int16_t>(regVal) * BUS_V_RESOLUTION_V;

    _voltage = busV;
    _current = shuntV / _shunt;

  } while(0);

  return rval;
}

/*!
  Get latest voltage/current values for selected channel (previously read with measurePower())

  \param[out] voltage Bus voltage for channel
  \param[out] current Shunt current for channel
*/
void INA232::getPower(float &voltage, float &current) {
  voltage = _voltage;
  current = _current;
}

/*!
  Get the devices I2C address

  \param[out] the I2C address of the INA
*/
uint16_t INA232::getAddr() {
  return _addr;
}
