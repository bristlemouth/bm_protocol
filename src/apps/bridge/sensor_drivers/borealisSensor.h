#pragma once

#include "abstractSensor.h"
#include "util.h"

//TODO: Remove this when codec is in place
#include "sensor_header_msg.h"
namespace BorealisDataMsg {

constexpr uint32_t VERSION = 1;
constexpr size_t NUM_FIELDS = 6 + SensorHeaderMsg::NUM_FIELDS;

struct Data {
  SensorHeaderMsg::Data header;
};

CborError decode(Data &d, const uint8_t *cbor_buffer, size_t size);

} // namespace BorealisDataMsg

typedef struct BorealisSensor : public AbstractSensor {
public:
  static constexpr uint32_t DEFAULT_BOREALIS_READING_PERIOD_MS = 1000;
  bool subscribe() override;

private:
  static void borealisSubCallback(uint64_t node_id, const char *topic, uint16_t topic_len,
                                  const uint8_t *data, uint16_t data_len, uint8_t type,
                                  uint8_t version);
  static constexpr char subtag[] = "/aos/borealis/spectrum";
} Borealis_t;

Borealis_t *createBorealisSensorSub(uint64_t node_id);
