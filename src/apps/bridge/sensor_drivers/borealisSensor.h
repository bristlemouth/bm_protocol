#pragma once

#include "abstractSensor.h"
#include "util.h"

typedef struct BorealisSensor : public AbstractSensor {
public:
  static constexpr uint32_t DEFAULT_BOREALIS_READING_PERIOD_MS = 1000;
  bool subscribe() override;

private:
  static void borealisSubCallback(uint64_t node_id, const char *topic, uint16_t topic_len,
                                  const uint8_t *data, uint16_t data_len, uint8_t type,
                                  uint8_t version);
  static constexpr char subtag_spectrum[] = "/aos/borealis/spectrum";
  static constexpr char subtag_levels[] = "/aos/borealis/levels";
} Borealis_t;

Borealis_t *createBorealisSensorSub(abstractSensorType_e type, uint64_t node_id);
