//
// Created by Genton Mo on 2019-07-19.
// Ported by Alvaro
// Ported by Victor (To bm_protocol)
//

#ifndef WS_APPS_TSYS01_H
#define WS_APPS_TSYS01_H

#include "stm32u5xx_hal.h"
#include "io.h"
#include "math.h"
#include "protected_spi.h"

#define RESET 0x1E
#define START_ADC_TEMP_CONV 0x48
#define READ_ADC_TEMP 0x00
#define PROM_ADDR_0 0xA0
#define SN_ADDR_0 0xAC
#define SN_ADDR_1 0xAE

#define RESET_WAIT_TIME_MS 6      // 2.8 ms x2 for assurance
#define ADC_CONV_WAIT_TIME_MS 10   // 8.22 ms + 1 ms for assurance
#define PROM_ADDR_CNT 8
#define CALIB_CNT 5

#define CALIB_4_CONST (-2 * 1e-21)
#define CALIB_3_CONST (4 * 1e-16)
#define CALIB_2_CONST (-2 * 1e-11)
#define CALIB_1_CONST (1 * 1e-6)
#define CALIB_0_CONST (-1.5 * 1e-2)
#define SN_MAJOR_SCALING_CONST (256)

#define TEMP_MAX 125
#define TEMP_MIN (-40)
#define TSYS01_INVALID_TEMP (-100)

class TSYS01 {
public:
  TSYS01(SPIInterface_t* spiInterface, IOPinHandle_t *cspin);

  bool begin( float offsetCentiDegC = 0.0, uint32_t signature = 0, uint32_t caltime = 0);
  void reset( void );
  bool getTemperature( float &temperature );
  bool checkPROM();
  bool getSerialNumber(uint32_t &sn);

private:

  SPIInterface_t* _SPIInterface;
  IOPinHandle_t* _CSPin;
  float _calibrations[5] = { CALIB_4_CONST, CALIB_3_CONST, CALIB_2_CONST, CALIB_1_CONST, CALIB_0_CONST };
  bool calibrated;
  float calibrationOffsetDegC;
  bool no_cal;

  bool validatePROM();
  bool doCommand( uint8_t command, uint32_t waitTimeMs );
  bool readData( uint8_t address, uint8_t data[] , size_t dataSize);
};

#endif //WS_APPS_TSYS01_H
