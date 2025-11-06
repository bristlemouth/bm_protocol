//
// Created by Uma Arthika Katikapalli on 8/20/25.
//
#pragma once
#include "OrderedSeparatorLineParser.h"
#include "aanderaa_conductivity_msg.h"
extern "C" {
#include "util.h"
}
#include <math.h>
#include <stdint.h>

#define CMD_STOP "Stop\r\n"
#define CMD_START "Start\r\n"
#define CMD_SET_PASSKEY_1 "Set Passkey(1)\r\n"
#define CMD_SET_PASSKEY_1000 "Set Passkey(1000)\r\n"

#define CMD_ENABLE_CONDUCTIVITY "Enable Conductivity"
#define CMD_ENABLE_SLEEP "Enable Sleep"
#define CMD_ENABLE_POLLEDMODE "Enable Polled Mode"
#define CMD_ENABLE_TEXT "Enable Text"
#define CMD_ENABLE_DECIMALFORMAT "Enable Decimalformat"
#define CMD_ENABLE_TEMPERATURE "Enable Temperature"
#define CMD_ENABLE_RAWDATA "Enable Rawdata"
#define CMD_ENABLE_DERIVEDPARAMETERS "Enable Derived Parameters"
#define CMD_ENABLE_RAWCOND1 "Enable RawCond1"
#define CMD_INTERVAL "Interval"
#define CMD_COMM_TIMEOUT "Comm TimeOut"
#define CMD_PRESSURE "Pressure"

#define CMD_YES "Yes"
#define CMD_NO "No"

#define CMD_GET "Get"
#define CMD_SET "Set"

#define CMD_SAVE "Save\r\n"
#define CMD_RESET "Reset\r\n"
#define ACK "#"
#define CMD_WAKE "\r\n"
#define CMD_GET_ALL "Get_All\r\n"
#define CMD_GET_ALL_PARAMS "Get_All Parameters\r\n"
#define CMD_SET_CELL_COEF "Set CellCoef(%f)\r\n"
#define CMD_GET_CELL_COEF "Get CellCoef\r\n"
#define CMD_GET_CONDUCTIVITY "Get Conductivity\r\n"
#define CMD_GET_SERIAL_NUMBER "Get Serial Number\r\n"

class AanderaaConductivitySensor {
public:
  AanderaaConductivitySensor() : _parser("\t", 256, PARSER_VALUE_TYPE, 5) {};

  void init();
  void configureSensor(void);
  bool getData(AanderaaConductivityMsg::Data &d);
  void clearPayloadBuffer(void);
  void resetSensor(void);
  void startStreaming(void);
  void calibrateCellCoef(void);
  bool checkAssignEpochValues(void);

  static constexpr char AANDERAA_CONDUCTIVITY_RAW_LOG[] = "aanderaa_salinity_raw.log";
  static constexpr char AANDERAA_CONDUCTIVITY_LOG[] = "aanderaa_salinity.log";

private:
  typedef uint32_t AanderaaConductivityUint;
  typedef float AanderaaConductivityFloat;
  typedef char AanderaaConductivityString[64];

  typedef struct {
    AanderaaConductivityUint serial_number;
    AanderaaConductivityFloat cell_coef;
  } ProductionConfigs;

  static constexpr uint32_t BAUD_RATE = 9600;
  static constexpr char LINE_TERM = '\n';

  //conductivity  temperature  salinity  waterdensity  soundspeed depth
  static constexpr ValueType PARSER_VALUE_TYPE[] = {TYPE_DOUBLE, TYPE_DOUBLE, TYPE_DOUBLE,
                                                    TYPE_DOUBLE, TYPE_DOUBLE};
  static constexpr char SENSOR_BM_LOG_ENABLE[] = "sensorBmLogEnable";
  static constexpr char SENSOR_DEPTH_M[] = "sensorDepthM";
  static constexpr char SENSOR_INTERVAL_S[] = "readingPeriodS";
  static constexpr char EXTERNAL_REFERENCE_CONDUCTIVITY[] = "referenceConductivity";
  static constexpr char CELL_COEF[] = "cellCoef";
  static constexpr char LAST_CAL_TIME_EPOCH_S[] = "lastCalTimeEpochS";
  static constexpr char CAL_COUNT[] = "calibrationCount";
  static constexpr char SENSOR_SERIAL_NUMBER[] = "sensorSerialNum";
  static constexpr char ERR_SERIAL_NUM[] = "errDetectedSensorSerialNum";

  static constexpr char FIRST_CAL_TIME_EPOCH_S[] = "firstCalTimeEpochS";
  static constexpr char FACTORY_CELL_COEF[] = "factoryCellCoef";

  uint32_t _sensorBmLogEnable = 0;
  float _sensorDepthM = 0.0f;
  float _pressureKpa = 0.0f;
  uint32_t _readingPeriodS = 2;
  OrderedSeparatorLineParser _parser;
  char _payload_buffer[2048];
  AanderaaConductivityUint _serialNumber;
  bool _serialNumberErrSet = false;

  float _cellCoef = 0.000000f;
  float _referenceConductivity = NAN;
  float _measuredConductivity = NAN;
  bool _sensorConfigDirty = false;

  // The save procedure may take up to 20 seconds according to Table 5-2 in the
  // TD321 Operation Manual
  static constexpr uint16_t _saveTimeMs = 20000;

  void validateSerialNumber(const char *str);

  bool hasProductionConfigs(ProductionConfigs &production_configs);
  void checkAssignProductionConfigs(void);

  void checkTypeAndAssign(const char *output, uint16_t length, AanderaaConductivityUint *value);
  void checkTypeAndAssign(const char *output, uint16_t length,
                          AanderaaConductivityFloat *value);
  void checkTypeAndAssign(const char *output, uint16_t length,
                          AanderaaConductivityString *value);

  BmErr sendCommand(const char *command, uint32_t timeout_ms = 1000);
  template <typename T>
  BmErr sendCommand(const char *command, T *value = nullptr, uint32_t timeout_ms = 1000);

  bool compareValuesPopulateBuffer(const char *parameter, AanderaaConductivityString *buf,
                                   const AanderaaConductivityUint read,
                                   const AanderaaConductivityUint validate);
  bool compareValuesPopulateBuffer(const char *parameter, AanderaaConductivityString *buf,
                                   const AanderaaConductivityFloat read,
                                   const AanderaaConductivityFloat validate);
  bool compareValuesPopulateBuffer(const char *parameter, AanderaaConductivityString *buf,
                                   const AanderaaConductivityString read,
                                   const AanderaaConductivityString validate);

  BmErr readValidateWriteValue(const char *parameter, const char *expected_val,
                               uint8_t retries = 3);
  template <typename T>
  BmErr readValidateWriteValue(const char *parameter, T expected_val, uint8_t retries = 3);
};
