#pragma once
#include "OrderedSeparatorLineParser.h"
#include "bm_rbr_data_msg.h"
#include <stdint.h>

class RbrSensor {
public:
  RbrSensor()
      : _type(BmRbrDataMsg::SensorType_t::UNKNOWN),
        _stored_type(BmRbrDataMsg::SensorType_t::UNKNOWN), _sensorDropDebounceCount(0),
        _parserTwoArguments(",", 256, parserValueTypeOne, 2),
        _parserThreeArguments(",", 256, parserValueTypeTwo, 3){};
  void init(BmRbrDataMsg::SensorType_t type);
  bool probeType(uint32_t timeout_ms = 1000);
  bool getData(BmRbrDataMsg::Data &d);
  void flush(void);

public:
  static constexpr char RBR_RAW_LOG[] = "rbr_raw.log";

private:
  static constexpr uint32_t BAUD_RATE = 9600;
  static constexpr char LINE_TERM = '\n';
  static constexpr ValueType parserValueTypeOne[] = {TYPE_UINT64, TYPE_DOUBLE};
  static constexpr ValueType parserValueTypeTwo[] = {TYPE_UINT64, TYPE_DOUBLE, TYPE_DOUBLE};
  static constexpr char typeCommand[] = "outputformat channelslist\n";
  static constexpr uint8_t SENSOR_DROP_DEBOUNCE_MAX_COUNT = 3;
  static constexpr uint8_t NUM_PARSERS = 4;

private:
  BmRbrDataMsg::SensorType_t _type;
  BmRbrDataMsg::SensorType_t _stored_type;
  uint8_t _sensorDropDebounceCount = 0;
  OrderedSeparatorLineParser _parserTwoArguments;
  OrderedSeparatorLineParser _parserThreeArguments;
  OrderedSeparatorLineParser *_parsers[NUM_PARSERS] = {
      // Defined the order of BmRbrDataMsg
      NULL,                  // BmRbrDataMsg::SensorType_t::UNKNOWN
      &_parserTwoArguments,  // time + BmRbrDataMsg::SensorType_t::TEMPERATURE
      &_parserTwoArguments,  // time + BmRbrDataMsg::SensorType_t::PRESSURE
      &_parserThreeArguments // time + BmRbrDataMsg::SensorType_t::TEMPERATURE_AND_PRESSURE
  };
  char _payload_buffer[2048];
};
