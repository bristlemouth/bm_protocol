#pragma once
#include "OrderedSeparatorLineParser.h"
#include "pme_dissolved_oxygen_msg.h"
#include "pme_wipe_msg.h"
#include <stdint.h>

void saveLastWipeEpoch(uint32_t epochTimeSec);
uint32_t loadLastWipeEpoch(void);
void ledAllOff(void);

class PmeSensor {
public:
  PmeSensor(void)
      : _DOTparser(",", 256, DOT_PARSER_VALUE_TYPE, 5),
        _WIPEparser(",", 256, WIPE_PARSER_VALUE_TYPE, 6) {};
  void init(void);
  bool getDoData(PmeDissolvedOxygenMsg::Data &d, float salinityPpt);
  bool getWipeData(PmeWipeMsg::Data &w);
  bool getSN(PmeDissolvedOxygenMsg::Data &s);
  void flush(void);
  void setBarometricPressure(double pressure);

  static constexpr char PME_DO_RAW_LOG[] = "pme_do_raw.log";
  static constexpr char PME_WIPE_RAW_LOG[] = "pme_wipe_raw.log";

private:
  static constexpr ValueType DOT_PARSER_VALUE_TYPE[] = {TYPE_DOUBLE, TYPE_DOUBLE, TYPE_DOUBLE,
                                                        TYPE_DOUBLE, TYPE_DOUBLE};
  static constexpr ValueType WIPE_PARSER_VALUE_TYPE[] = {TYPE_DOUBLE, TYPE_DOUBLE, TYPE_DOUBLE,
                                                         TYPE_DOUBLE, TYPE_DOUBLE, TYPE_DOUBLE};

  OrderedSeparatorLineParser _DOTparser;
  OrderedSeparatorLineParser _WIPEparser;
  char _DOTpayload_buffer[2048];
  char _WIPEpayload_buffer[2048];
  char _SNpayload_buffer[2048];
  bool _is_baro_valid = false;
  double _barometric_pressure_mbar = 1013.25;
};
