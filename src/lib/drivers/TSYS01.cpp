//
// Created by Genton Mo on 2019-07-19.
// Ported by Alvaro
//

#include "FreeRTOS.h"
#include "task.h"
#include "TSYS01.h"
#include "debug.h"

TSYS01::TSYS01(SPIInterface_t* spiInterface, IOPinHandle_t *cspin):
_SPIInterface(spiInterface),_CSPin(cspin)
{
  IOWrite(_CSPin, 1);
}

/* Initialize TSYS01.
  \param[in] offsetDegC - SST offset in degrees C
  \param[in] signature - SST calibration signature
  \return Will return false if sensor is not present or communicating
*/

bool TSYS01::begin( float offsetCentiDegC, uint32_t signature, uint32_t caltime ) {
  // Make sure CS pin is high before starting
  uint32_t sn;
  IOWrite(_CSPin, 1);
  reset();
  bool rval = validatePROM();
  rval &= getSerialNumber(sn);

  calibrationOffsetDegC = 0.0;
  calibrated = false;
  no_cal = false;

  if (!rval) {
    printf("TSYS01 init failed\n");
    calibrated = false;
  } else if (!signature && !caltime && !offsetCentiDegC) {
    printf("TSYS01 no cal during init\n");
    calibrated = false;
    no_cal = true;
  } else if (signature && caltime && signature == sn) {
    printf("TSYS01 init success\n");
    calibrationOffsetDegC = offsetCentiDegC / 100.0f;
    calibrated = true;
  } else {
    printf("TSYS01 invalid calibration error during init\n");
    calibrated = false;
  }

  return rval;
}

void TSYS01::reset( void ) {
  doCommand(RESET, RESET_WAIT_TIME_MS);
}

bool TSYS01::getTemperature( float &temperature ) {
  float temp = 0;
  uint8_t data[3] = { 0 };
  bool spiTransactionSuccess = true;
  bool rval = true;

  spiTransactionSuccess &= doCommand(START_ADC_TEMP_CONV, ADC_CONV_WAIT_TIME_MS);
  spiTransactionSuccess &= readData(READ_ADC_TEMP, data, sizeof(data));

  // round() macro returns a long (4 bytes)
  uint32_t adc16 = round(((float) (((uint32_t) data[0]) << 16 | ((uint32_t) data[1]) << 8 | data[2])) / 256);
  for( uint8_t i = 0; i < CALIB_CNT; i++){
    temp += _calibrations[i] * pow(adc16, 4 - i);
  }

  if( spiTransactionSuccess && temp > TEMP_MIN && temp < TEMP_MAX){
    temperature = temp + calibrationOffsetDegC;
    if (no_cal){
      printf("TSYS01 no cal during reading\n");
    } else if (!calibrated) {
      printf("TSYS01 invalid calibration error during reading\n");
    }
  } else {
    printf("TSYS01 reading error\n");
    rval = false;
  }

  return rval;
}

bool TSYS01::validatePROM() {
  uint16_t checksum = 0;
  uint8_t data[2] = { 0 };
  bool spiTransactionSuccess = true;

  // Reset calibrations
  _calibrations[0] = CALIB_4_CONST;
  _calibrations[1] = CALIB_3_CONST;
  _calibrations[2] = CALIB_2_CONST;
  _calibrations[3] = CALIB_1_CONST;
  _calibrations[4] = CALIB_0_CONST;

  for( uint8_t i = 0; i < PROM_ADDR_CNT; i++ ){
    spiTransactionSuccess &= readData(PROM_ADDR_0 + (i * 2), data, sizeof(data));
    checksum += data[0] + data[1];
    if( i && i < 6 ){
      uint16_t kCoeff = ((uint16_t) data[0]) << 8 | data[1];
      _calibrations[i-1] *= kCoeff;
    }
  }
  return spiTransactionSuccess && checksum && !(checksum & 0xFF);
}

/*!
  Make sure reading prom works. On failure, it will set the error state.
  This can be run anytime to check for SPI bus corruption. (If we get random
  data back from SPI bus, it could show up as valid temperature, but the
  PROM checksum will most likely fail to validate.)

  \return true if successfull false otherwise
*/
bool TSYS01::checkPROM() {
  bool rval = validatePROM();

  if (!rval) {
    printf("TSYS01 reading error while checking PROM\n");
  } else {
    printf("TSYS01 PROM check passed\n");
  }

  return rval;
}

bool TSYS01::doCommand( uint8_t command, uint32_t waitTimeMs ){
  SPIResponse_t status;
  uint8_t dummyRxByte; // SPI transmits and receives at the same time

  status = spiTxRx(_SPIInterface, _CSPin, 1, &command, &dummyRxByte, 10);
  vTaskDelay(pdMS_TO_TICKS(waitTimeMs));

  return (status == SPI_OK);
}

bool TSYS01::readData( uint8_t address, uint8_t data[], size_t dataSize) {
  uint8_t txData[sizeof(address) + dataSize];
  uint8_t rxData[sizeof(address) + dataSize];
  SPIResponse_t status;

  txData[0] = address;

  for(uint32_t index=0; index < dataSize; index++) {
    txData[1 + index] = data[index];
  }

  status = spiTxRx(_SPIInterface, _CSPin, sizeof(txData), txData, rxData, 10);

  for(uint32_t index=0; index < dataSize; index++) {
    data[index] = rxData[1 + index];
  }

  return (status == SPI_OK);
}

/* Get the serial number as per:
 https://www.te.com/commerce/DocumentDelivery/DDEController?Action=srchrtrv&DocNm=TSYS01&DocType=Data+Sheet&DocLang=English
 Pg. 9
 \param[out] TSYS01 serial number

 \return bool if succesfull, false otherwise
*/
bool TSYS01::getSerialNumber(uint32_t &sn) {
  bool rval = true;
  uint8_t data[2] = {0};
  rval &= readData(SN_ADDR_0, data, sizeof(data));
  uint16_t sn_major = data[0] << 8 | data[1];
  rval &= readData(SN_ADDR_1, data, sizeof(data));
  uint8_t sn_minor = data[0];
  sn = (SN_MAJOR_SCALING_CONST * sn_major) + sn_minor;
  return rval;
}
