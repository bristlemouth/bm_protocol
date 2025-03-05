#include "BQ25820.h"
#include "debug.h"
#include "app_util.h"

using namespace BQ;

#define PN_REV 0x1A
#define BQ_RST (1 << 7)

static const uint16_t inaAvgCounts[] = {1, 4, 16, 64, 128, 256, 512, 1024};
static const uint16_t inaConvTimes[] = {140, 203, 332, 588, 1100, 2116, 4156, 8244};

BQ25820::BQ25820(I2CInterface_t * interface, uint8_t address)
{
  _interface = interface;
  _addr = static_cast<uint8_t>(address);
}

/*!
  Initialize the battery charger. Check that it is present and set default configuration.

  \return true if successfull false otherwise
*/
bool BQ25820::init() {
  bool rval = false;
  uint16_t reg = 0;
  printf("INA232 Init\n");

  uint8_t retriesRemaining = 3;
  while (!rval && retriesRemaining--) {
    printf("Retries remaining %u\n", retriesRemaining);
    if(!readReg(PART_INFO_REG, &reg)) {
      continue;
    }
    printf("MFG_ID: %04X\n", reg);

    if(reg != PN_REV) {
      printf("Invalid manufacturer pn or rev\n");
      continue;
    }

    // Reset to defaults
    reg = BQ_RST;
    if(!write8(POWER_PATH_REG, reg)) {
      continue;
    }

    if(!read8(POWER_PATH_REG, &reg)) {
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
bool BQ25820::readReg(Reg_t reg, uint16_t *value) {
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
  Read 8-bit register from device

  \param[in] reg Register address
  \param[out] *value register value
  \return true if successfull false otherwise
*/
bool BQ25820::read8(Reg_t reg, uint16_t *value) {
  bool rval = false;
  uint8_t regByte = reg;
  uint8_t regVal = 0;

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
bool BQ25820::writeReg(Reg_t reg, uint16_t value) {
  uint8_t bytes[] = {static_cast<uint8_t>(reg), static_cast<uint8_t>(value >> 8), static_cast<uint8_t>(value & 0xFF)};
  return (writeBytes(bytes, sizeof(bytes), 100) == I2C_OK);
}

/*!
  Write 8-bit register to device

  \param[in] reg Register address
  \param[in] value Register value to set
  \return true if successfull false otherwise
*/
bool BQ25820::write8(Reg_t reg, uint8_t value) {
  uint8_t bytes[] = {static_cast<uint8_t>(reg), value};
  return (writeBytes(bytes, sizeof(bytes), 100) == I2C_OK);
}

/*!
  Set bits in configuration register

  \param[in] bits Value to set in config
  \param[in] mask Mask for value size (since it will have to clear the bits first)
  \param[in] position Number of bits to left shift before clearing/setting value
  \return true if successfull false otherwise
*/
bool BQ25820::disablePfm() {
  bool rval = false;

  uint8_t tmpreg;
  do {

    if(!read8(POWER_PATH_REG, &tmpreg)) {
      break;
    }

    // Clear pfm bit (the 5th bit)
    tmpreg &= 0x1 << 4;

    if(!write8(REG_CFG, tmpreg)) {
      break;
    }

    rval = true;
  } while (0);

  return rval;

}

