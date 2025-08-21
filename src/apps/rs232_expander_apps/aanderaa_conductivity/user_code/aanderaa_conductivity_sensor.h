//
// Created by Uma Arthika Katikapalli on 8/20/25.
//
#pragma once
#include <stdint.h>
#include "OrderedSeparatorLineParser.h"
#include "aanderaa_conductivity_msg.h"

class AanderaaConductivitySensor {
public:
    AanderaaConductivitySensor()
        : _parser(",", 256, PARSER_VALUE_TYPE, 6) {};
    void init();
    bool getData(AanderaaConductivityMsg::Data &d);
    void flush(void);

public:
    static constexpr char AANDERAA_CONDUCTIVITY_RAW_LOG[] = "aanderaa_conductivity_raw.log";

private:
    static constexpr uint32_t BAUD_RATE = 9600;
    static constexpr char LINE_TERM = '\n';
    static constexpr ValueType PARSER_VALUE_TYPE[] = {
        TYPE_DOUBLE, TYPE_DOUBLE, TYPE_DOUBLE, TYPE_DOUBLE, TYPE_DOUBLE, TYPE_FLOAT};
    static constexpr char SENSOR_BM_LOG_ENABLE[] = "sensorBmLogEnable";

private:
    uint32_t _sensorBmLogEnable = 0;
    OrderedSeparatorLineParser _parser;
    char _payload_buffer[2048];
    /*
    AanderaaConductivityMsg::Data _data; // Store the last read data
    bool _data_available = false; // Flag to indicate if data is available
    bool _data_parsed = false; // Flag to indicate if data has been parsed
    bool _data_valid = false; // Flag to indicate if the parsed data is valid
    bool _data_sent = false; // Flag to indicate if the data has been sent
    bool _data_error = false; // Flag to indicate if there was an error in data
    bool _data_flush = false; // Flag to indicate if the data should be flushed
    bool _data_init = false; // Flag to indicate if the sensor has been initialized
    */
};
