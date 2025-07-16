#include "bq25820.h"
#include "debug.h"
#include "app_util.h"

using namespace BQ;

#define PN_REV 0x1A
#define BQ_RST (1 << 7)
#define POWERPATH_REG_DEF 0x20

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
  uint8_t reg = 0;
  printf("BQ25820 init\n");

  uint8_t retriesRemaining = 3;
  while (!rval && retriesRemaining--) {
    printf("Retries remaining %u\n", retriesRemaining);
    if(!read8(PART_INFO_REG, &reg)) {
      continue;
    }
    printf("MFG_ID: %04X\n", reg);

    if(reg != PN_REV) {
      printf("Invalid manufacturer pn or rev\n");
      continue;
    }

    vTaskDelay(pdMS_TO_TICKS(2));

    // Reset to defaults
    reg = BQ_RST;
    if(!write8(POWER_PATH_REG, reg)) {
      continue;
    }

    vTaskDelay(pdMS_TO_TICKS(2));

    reg = 0x00;
    if(!read8(POWER_PATH_REG, &reg)) {
      continue;
    }
    
    if((reg == POWERPATH_REG_DEF || reg == (POWERPATH_REG_DEF | 0x2))) {
        printf("Wrong powerpath reg value after reset!\n");
        printf("reg_val: %04X\n", reg);
        vTaskDelay(pdMS_TO_TICKS(2));
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
bool BQ25820::read16(Reg_t reg, int16_t *value) {
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

    //if(value != NULL) {
    //  *value = __builtin_bswap16(regVal);
    //}
    *value = regVal;

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
bool BQ25820::read8(Reg_t reg, uint8_t *value) {
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
      *value = regVal;
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
bool BQ25820::write16(Reg_t reg, uint16_t value) {
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

  //return true;
  printf("Disabling PulseFrequencyModulation\n");

  uint8_t tmpreg;
  do {

    if(!read8(POWER_PATH_REG, &tmpreg)) {
      break;
    }

    // Clear pfm bit (the 5th bit)
    tmpreg &= ~(0x1 << 5);

    if(!write8(POWER_PATH_REG, tmpreg)) {
      break;
    }
    printf("power path reg updated\n");

    tmpreg = 0;
    if(!read8(POWER_PATH_REG, &tmpreg)) {
        break;
    }
    printf("PowerPathReg: %04X\n", tmpreg);

    rval = true;
  } while (0);

  return rval;
}

bool BQ25820::readSensors(char *buffer, size_t len) {
  bool rval = false;
  bool quit = false;
  
  int16_t iac_val;
  int16_t iac;

  int16_t ibat_val;
  int16_t ibat;
  
  int16_t vac_val;
  float vac;
  
  int16_t vbat_val;
  float vbat;
  
  int16_t vbat_fb_val;
  float vbat_fb;
  
  int16_t vsys_val;
  float vsys;
  
  int16_t ts_val;
  float ts;

  uint8_t ppreg = 0x00;

  uint8_t tmpreg;

  uint16_t i;

  write8(ADC_CH_CTRL_REG, 0x00);

  do {
    if(!write8(ADC_CTRL_REG, 0xE0)) { // 0xA8 = old val
      break;
    }
    for (i = 0; i < 1000; i++) {
      //printf("%d\n", i);
      vTaskDelay(pdMS_TO_TICKS(2));
      if(!read8(CHARGER_FLAG_1, &tmpreg)) { 
        quit = true; // Tell the outer do while to exit
        printf("Charger flag 1 read failed\n");
        break; // Exit this polling loop
      }
      if (tmpreg && 0x80) {
        printf("Reading ADCs\n");
        //vTaskDelay(pdMS_TO_TICKS(150));
        printf("loops: %d\n", i);
        // ADC is done
        //vTaskDelay(pdMS_TO_TICKS(5));

        read16(IAC_ADC_REG, &iac_val);
        iac = iac_val * 2;
        printf("%d,", iac);

        read16(IBAT_ADC_REG, &ibat_val);
        ibat = ibat_val * 2;
        printf("%d,", ibat);
        
        read16(VAC_ADC_REG, &vac_val);
        vac = vac_val * 0.002f;
        printf("%.3f,", vac);
        
        read16(VBAT_ADC_REG, &vbat_val);
        vbat = vbat_val * 0.002f;
        printf("%.3f,", vbat);
        
        read16(VFB_ADC_REG, &vbat_fb_val);
        vbat_fb = vbat_fb_val * 0.008691f;
        printf("%.3f,", vbat_fb);
        
        read16(VSYS_ADC_REG, &vsys_val);
        vsys = vsys_val * 0.002f;
        printf("%.3f,", vsys);
        
        read16(TS_ADC_REG, &ts_val);
        ts = ts_val * 0.09765625f;
        //printf("%.3f\n", ts);
        printf("%.3f,", ts);

        read8(POWER_PATH_REG, &ppreg);
        //printf("0x%X\n", ppreg);

        snprintf(buffer, len, "%d,%d,%.3f,%.3f,%.3f,%.3f,%.3f,", iac, ibat,
            vac, vbat, vbat_fb, vsys, ts);
      }
      vTaskDelay(pdMS_TO_TICKS(5));
      //break;
    }
    if (quit) {
      break;
    }
    rval = true;
  } while(0);
  
  read8(CHARGER_CTRL_REG, &tmpreg);
  printf("0x%X\n", tmpreg);

  return rval;
}


bool BQ25820::readFaults(char *buffer, size_t len) {
  uint8_t faults = 0x00;
  int cx = 0;

  char names[8][30] = {"RESERVED", "DRV_OKZ_FLAG", "CHG_TMR_FLAG", "TSHUT_FLAG", "VBAT_OV_FLAG", "IBAT_OCP_FLAG", "VAC_OV_FLAG", "VAC_UV_FLAG"};
  read8(FAULT_FLAG_REG, &faults);
  
  //printf("%b:\t", faults);

  for (uint8_t i=0; i < 8; i++) {
    //printf("%u, ", i);
    if (faults & (1 << i)) {
      printf("%s ", names[i]);
      cx = snprintf(buffer+cx, len-cx, "%s ", names[i]);
      if (cx<0 || cx>=(int)len) {
        printf("String length error!!\n");
        break;
      }
    }
  }
  printf("\n");
  return true;
}
