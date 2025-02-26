#pragma once
#include "OrderedSeparatorLineParser.h"
#include "pme_dissolved_oxygen_msg.h"
#include "pme_wipe_msg.h"
#include <stdint.h>

void saveLastWipeEpoch(uint32_t epochTimeSec);
uint32_t loadLastWipeEpoch();
void ledAllOff();

class PmeSensor {
public:
  PmeSensor()
      : _DOTparser(",", 256, DOT_PARSER_VALUE_TYPE, 5),
        _WIPEparser(",", 256, WIPE_PARSER_VALUE_TYPE, 6) {};
  void init();
  bool getDoData(PmeDissolvedOxygenMsg::Data &d);
  bool getWipeData(PmeWipeMsg::Data &w);
  bool getSN(PmeDissolvedOxygenMsg::Data &s);
  void flush();

public:
  static constexpr char PME_DO_RAW_LOG[] = "pme_do_raw.log";
  static constexpr char PME_WIPE_RAW_LOG[] = "pme_wipe_raw.log";

private:
  static constexpr uint32_t BAUD_RATE = 9600;
  static constexpr char LINE_TERM = '\r';
  static constexpr ValueType DOT_PARSER_VALUE_TYPE[] = {TYPE_DOUBLE, TYPE_DOUBLE, TYPE_DOUBLE,
                                                        TYPE_DOUBLE, TYPE_DOUBLE};
  static constexpr ValueType WIPE_PARSER_VALUE_TYPE[] = {TYPE_DOUBLE, TYPE_DOUBLE, TYPE_DOUBLE,
                                                         TYPE_DOUBLE, TYPE_DOUBLE, TYPE_DOUBLE};

private:
  OrderedSeparatorLineParser _DOTparser;
  OrderedSeparatorLineParser _WIPEparser;
  char _DOTpayload_buffer[2048];
  char _WIPEpayload_buffer[2048];
  char _SNpayload_buffer[2048];
};
