#pragma once
#include <stdint.h>
#include "OrderedSeparatorLineParser.h"
#include "aanderaa_conductivity_msg.h"
#include "AanderaaConductivitySensorUtil.h"

class AanderaaConductivitySensor {
  public:
    AanderaaConductivitySensor()
        : _parser(",", 256, PARSER_VALUE_TYPE, 6){};
    void init();
    bool getData(AanderaaConductivityMsg::Data &d);
    void flush(void);

  public:
    static constexpr char AANDERAA_CONDUCTIVITY_RAW_LOG[] = "aanderaa_conductivity_raw.log";

  private:
    static constexpr uint32_t BAUD_RATE = 115200;
    static constexpr char LINE_TERM = '\n';
    static constexpr ValueType PARSER_VALUE_TYPE[] = {TYPE_DOUBLE, TYPE_DOUBLE, TYPE_DOUBLE, TYPE_DOUBLE, TYPE_DOUBLE, TYPE_DOUBLE};
    static constexpr char SENSOR_BM_LOG_ENABLE[] = "sensorBmLogEnable";

  private:
    uint32_t _sensorBmLogEnable = 0;
    OrderedSeparatorLineParser _parser;
    char _payload_buffer[2048];
};
