//
// Created by Uma Arthika Katikapalli on 8/20/25.
//
#pragma once
#include <stdint.h>
#include "OrderedSeparatorLineParser.h"
#include "aanderaa_conductivity_msg.h"

#define CMD_STOP                        "Stop\r\n"
#define CMD_START                       "Start\r\n"
#define CMD_SET_PASSKEY_1               "Set Passkey(1)\r\n"
#define CMD_ENABLE_CONDUCTIVITY_YES     "Set Enable Conductivity(Yes)\r\n"
#define CMD_ENABLE_SLEEP_YES            "Set Enable Sleep(Yes)\r\n"
#define CMD_ENABLE_POLLEDMODE_NO        "Set Enable Polled Mode(No)\r\n"
#define CMD_ENABLE_TEXT_YES             "Set Enable Text(Yes)\r\n"
#define CMD_ENABLE_TEXT_NO              "Set Enable Text(No)\r\n"
#define CMD_ENABLE_DECIMALFORMAT_YES    "Set Enable Decimalformat(Yes)\r\n"
#define CMD_ENABLE_TEMPERATURE_YES      "Set Enable Temperature(Yes)\r\n"
#define CMD_ENABLE_RAWDATA_NO           "Set Enable Rawdata(No)\r\n"
#define CMD_ENABLE_DERIVEDPARAMETERS_YES "Set Enable Derived Parameters(Yes)\r\n"
#define CMD_ENABLE_DERIVEDPARAMETERS_NO "Set Enable Derived Parameters(No)\r\n"
#define CMD_ENABLE_RAWCOND1_NO          "Set Enable RawCond1(No)\r\n"
#define CMD_SET_INTERVAL                "Set Interval(%" PRIu32 ")\r\n"
#define CMD_SAVE                        "Save\r\n"
#define CMD_RESET                       "Reset\r\n"
#define ACK                             "#\r\n"
#define CMD_GET_ALL                     "Get_All\r\n"
#define CMD_GET_ALL_PARAMS              "Get_All Parameters\r\n"
#define CMD_SET_PRESSURE                "Set Pressure(%.2f)\r\n"
#define CMD_COMM_TIMEOUT               "Set Comm TimeOut(1 min)\r\n"


class AanderaaConductivitySensor {
public:
    AanderaaConductivitySensor()
        : _parser("\t", 256, PARSER_VALUE_TYPE, 5) {};
    void init();
    void configureSensor(void);
    bool getData(AanderaaConductivityMsg::Data &d);
    void flush(void);
    void clearPayloadBuffer(void);
    void resetSensor(void);
    void startStreaming(void);

    static constexpr char AANDERAA_CONDUCTIVITY_RAW_LOG[] = "aanderaa_conductivity_raw.log";

private:
    static constexpr uint32_t BAUD_RATE = 9600;
    static constexpr char LINE_TERM = '\n';

    //conductivity  temperature  salinity  waterdensity  soundspeed depth
    static constexpr ValueType PARSER_VALUE_TYPE[] = {TYPE_DOUBLE, TYPE_DOUBLE, TYPE_DOUBLE, TYPE_DOUBLE, TYPE_DOUBLE};
    static constexpr char SENSOR_BM_LOG_ENABLE[] = "sensorBmLogEnable";
    static constexpr char SENSOR_DEPTH_M[] = "sensorDepthM";
    static constexpr char SENSOR_PRESSURE_KPA[] = "pressureKpa";
    static constexpr char SENSOR_INTERVAL_S[] = "readingIntervalS";
    static constexpr char SENSOR_CELL_COEFF[] = "cellCoeff";

    uint32_t _sensorBmLogEnable = 0;
    float _sensorDepthM = 0.0f;
    float _pressureKpa = 0.0f;
	uint32_t _intervalS = 2;
    uint32_t _cellCoeff = 0;
    OrderedSeparatorLineParser _parser;
    char _payload_buffer[2048];

};
