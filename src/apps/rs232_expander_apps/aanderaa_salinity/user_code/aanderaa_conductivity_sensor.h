//
// Created by Uma Arthika Katikapalli on 8/20/25.
//
#pragma once
#include "OrderedSeparatorLineParser.h"
#include "aanderaa_conductivity_msg.h"
#include "aanderaa_sensor.h"
#include <math.h>
#include <stdint.h>

#define CMD_ENABLE_CONDUCTIVITY "Enable Conductivity"
#define CMD_ENABLE_TEMPERATURE "Enable Temperature"
#define CMD_ENABLE_RAWDATA "Enable Rawdata"
#define CMD_ENABLE_DERIVEDPARAMETERS "Enable Derived Parameters"
#define CMD_ENABLE_RAWCOND1 "Enable RawCond1"
#define CMD_PRESSURE "Pressure"

#define CMD_SET_CELL_COEF "Set CellCoef(%f)\r\n"
#define CMD_GET_CELL_COEF "Get CellCoef\r\n"
#define CMD_GET_CONDUCTIVITY "Get Conductivity\r\n"

class AanderaaConductivitySensor : public AanderaaSensor {
public:
  AanderaaConductivitySensor() : _parser("\t", 256, PARSER_VALUE_TYPE, 5) {};

  void init();
  void configureSensor(void);
  bool getData(AanderaaConductivityMsg::Data &d);
  void clearPayloadBuffer(void);
  void calibrateCellCoef(void);
  bool checkAssignEpochValues(void);

  static constexpr char AANDERAA_CONDUCTIVITY_RAW_LOG[] = "aanderaa_salinity_raw.log";
  static constexpr char AANDERAA_CONDUCTIVITY_LOG[] = "aanderaa_salinity.log";

private:
  typedef struct {
    AanderaaUint serial_number;
    AanderaaFloat cell_coef;
  } ProductionConfigs;

  static constexpr uint32_t BAUD_RATE = 9600;

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
  AanderaaUint _serialNumber;
  bool _serialNumberErrSet = false;

  float _cellCoef = 0.000000f;
  float _referenceConductivity = NAN;
  float _measuredConductivity = NAN;

  void validateSerialNumber(const char *str);

  bool hasProductionConfigs(ProductionConfigs &production_configs);
  void checkAssignProductionConfigs(void);
};
