#pragma once
#include "OrderedSeparatorLineParser.h"
#include "bm_rbr_data_msg.h"
#include <stdint.h>

class RbrSensor {
public:
  RbrSensor()
      : _type(BmRbrDataMsg::SensorType_t::UNKNOWN),
        _stored_type(BmRbrDataMsg::SensorType_t::UNKNOWN), _sensorDropDebounceCount(0),
        _sensorBmLogEnable(false), _minProbePeriodMs(0), _lastProbeTime(0),
        _awaitingProbeResponse(false), _parserTwoArguments(",", 256, parserValueTypeOne, 2),
        _parserThreeArguments(",", 256, parserValueTypeTwo, 3){};
  void init(BmRbrDataMsg::SensorType_t type, uint32_t min_probe_period_ms);
  void maybeProbeType(uint64_t last_power_on_time);
  bool getData(BmRbrDataMsg::Data &d);
  void flush(void);

private:
  void handleOutputformat(const char *s, size_t read_len);
  bool handleDataString(const char *s, size_t read_len, BmRbrDataMsg::Data &d);

public:
  static constexpr char RBR_RAW_LOG[] = "rbr_raw.log";
  static constexpr char CFG_RBR_TYPE[] = "rbrCodaType";

private:
  static constexpr uint32_t BAUD_RATE = 1200;
  static constexpr char LINE_TERM = '\n';
  static constexpr ValueType parserValueTypeOne[] = {TYPE_UINT64, TYPE_DOUBLE};
  static constexpr ValueType parserValueTypeTwo[] = {TYPE_UINT64, TYPE_DOUBLE, TYPE_DOUBLE};
  static constexpr char typeCommand[] = "outputformat channelslist\n";
  static constexpr uint8_t SENSOR_DROP_DEBOUNCE_MAX_COUNT = 3;
  static constexpr uint32_t PROBE_TYPE_TIMEOUT_MS = 1000;
  static constexpr uint8_t NUM_PARSERS = 4;
  static constexpr char sensor_bm_log_enable[] = "sensorBmLogEnable";

private:
  BmRbrDataMsg::SensorType_t _type;
  BmRbrDataMsg::SensorType_t _stored_type;
  uint8_t _sensorDropDebounceCount = 0;
  uint32_t _sensorBmLogEnable = 0;
  uint32_t _minProbePeriodMs = 0;
  uint64_t _lastProbeTime = 0;
  bool _awaitingProbeResponse = false;
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
