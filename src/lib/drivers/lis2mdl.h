#ifndef __LIS2MDL_H__
#define __LIS2MDL_H__

#include "abstract_st_sensor.h"
#include "lsm6dsv.h"

class LIS2MDL : public AbstractSensorInterface {
public:
  typedef struct {
    lis2mdl_odr_t sample_rate;
    lis2mdl_lp_t mode;
  } Cfg;

  typedef struct {
    uint64_t timestamp_ns;
    float x;
    float y;
    float z;
  } LIS2MDLReading;

  using AbstractSensorInterface::AbstractSensorInterface;
  BmErr init(void) override;
  BmErr set_data(const uint8_t *buf, size_t len) override;
  BmErr get_reading(LIS2MDLReading *reading);

  LSM6DSV::LSM6DSVSensorHub m_sensor_hub = {
      .sensor = this,
      .reg = LIS2MDL_OUTX_L_REG,
      .len = EXPECTED_DATA_LENGTH,
  };

private:
  Cfg m_cfg = {
      .sample_rate = LIS2MDL_ODR_100Hz,
      .mode = LIS2MDL_HIGH_RESOLUTION,
  };

  static constexpr uint8_t EXPECTED_DATA_LENGTH = 6;

  static constexpr uint16_t DATA_SIZE_BYTES = 6;
  static constexpr uint16_t QUEUE_COUNT = 128;
  static constexpr uint16_t QUEUE_BUF_SIZE =
      (sizeof(QItem) + sizeof(LIS2MDLReading)) * QUEUE_COUNT;
  uint8_t m_readings_buf[QUEUE_BUF_SIZE];
  Q m_reading_queue;

  BmSemaphore m_queue_mut = NULL;
  ;
};

#endif
