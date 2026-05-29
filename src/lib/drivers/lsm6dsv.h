#ifndef __LSM6DSV_H__
#define __LSM6DSV_H__

#include "abstract_st_sensor.h"
#include "bm_os.h"
#include "q.h"
#include "util.h"
#include <span>
#include <stddef.h>
#include <stdint.h>

class LSM6DSV : public AbstractSensorInterface, public SensorInterfaceBus {
public:
  typedef struct {
    struct {
      lsm6dsv_xl_full_scale_t scale;
      lsm6dsv_xl_mode_t mode;
      lsm6dsv_data_rate_t rate;
    } accelerometer;
    struct {
      lsm6dsv_gy_full_scale_t scale;
      lsm6dsv_gy_mode_t mode;
      lsm6dsv_data_rate_t rate;
    } gyro;
  } Cfg;

  using AbstractSensorInterface::AbstractSensorInterface;
  virtual BmErr init(void) override { return init(NULL); }

  BmErr init(const Cfg *cfg);
  void handle_interrupt(void);

  typedef struct {
    AbstractSensorInterface *sensor = nullptr;
    uint8_t reg = 0;
    uint8_t len = 0;
  } LSM6DSVSensorHub;
  BmErr start_stream(std::span<LSM6DSVSensorHub> sensor_hub_items, size_t fifo_threshold,
                     bool sensor_hub_poll = false);
  BmErr stream_handle(void);

  typedef struct {
    float x;
    float y;
    float z;
  } LSM6DSVAccelerometer;

  typedef struct {
    float x;
    float y;
    float z;
  } LSM6DSVGyro;

  typedef struct __attribute__((__packed__)) {
    uint64_t ns; // reading timestamp in nanoseconds since boot of the LSM6DSV
    LSM6DSVAccelerometer acc;
    LSM6DSVGyro gyro;
    int16_t temp; // temperature in decidegree celsius
  } LSM6DSVReading;
  static_assert(sizeof(LSM6DSVReading) == 34);

  BmErr reading_ready(uint32_t timeout_ms);
  BmErr get_reading(LSM6DSVReading *reading);

  // Override inherited SensorInterfaceBus functions to support passthrough to sensor hub objects
  void begin() override;
  BmErr read(uint8_t reg, uint8_t *buf, size_t len, void *arg) override;
  BmErr write(uint8_t reg, const uint8_t *buf, size_t len, void *arg) override;
  void end() override;

private:
  // The following is sampled in this driver by default:
  //   - The accelerometer (sampled at 120Hz)
  //   - The gyro (sampled at 120Hz)
  //   - The timestamp (sampled at 120Hz)
  //   - The temperature (sampled at 1.875Hz)
  // The FIFO can hold 256 samples within it. Because of the 3 sensors sampled
  // at 120Hz there is a max of about 85 samples collected per unique sensor.
  // This value decreases as sensor hub items are added.
  static constexpr uint16_t QUEUE_COUNT = 85;
  static constexpr uint16_t QUEUE_BUF_SIZE =
      (sizeof(QItem) + sizeof(LSM6DSVReading)) * QUEUE_COUNT;

  uint8_t m_readings_buf[QUEUE_BUF_SIZE] = {};
  BmSemaphore m_queue_mut;
  Q m_reading_queue;

  Cfg m_cfg = {
      .accelerometer =
          {
              .scale = LSM6DSV_2g,
              .mode = LSM6DSV_XL_HIGH_PERFORMANCE_MD,
              .rate = LSM6DSV_ODR_AT_120Hz,
          },
      .gyro =
          {
              .scale = LSM6DSV_125dps,
              .mode = LSM6DSV_GY_HIGH_PERFORMANCE_MD,
              .rate = LSM6DSV_ODR_AT_120Hz,
          },
  };

  // Count for timestamp, gyro and accelerometer
  static constexpr uint8_t READING_COUNT = 3;

  struct {
    uint64_t resolution_ns = 0; // Resolution per bit in timestamp read
    uint64_t ns = 0;            // Current timestamp
    uint32_t last_count = 0;    // Last bit count in timestamp read
  } m_timestamp;

  int16_t m_temperature_dc = 0;

  BmSemaphore m_sensor_hub_mut;
  static constexpr uint8_t MAX_SENSOR_HUB_SENSORS = 4;
  struct {
    AbstractSensorInterface *sensor = nullptr;
    uint32_t nacks = 0;
  } m_sensor_hub[MAX_SENSOR_HUB_SENSORS];

  uint16_t m_streaming_sample_time_ms = 1000;
  BmSemaphore m_streaming_sem;
  BmSemaphore m_reading_sem;

  // Function to access sensor hub sensors
  void begin_sensor_hub(void);
  BmErr write_sensor_hub(uint8_t reg, const uint8_t *data, size_t len, void *arg);
  BmErr read_sensor_hub(uint8_t reg, uint8_t *data, size_t len, void *arg);
  void end_sensor_hub(void);

  typedef float (*ConverterCb)(int16_t);
  static ConverterCb accelerometer_convert(lsm6dsv_xl_full_scale_t scale);
  static ConverterCb gyro_convert(lsm6dsv_gy_full_scale_t scale);
};

#endif
