#ifndef __LSM6DSV_H__
#define __LSM6DSV_H__

#include "bm_os.h"
#include "lsm6dsv_reg.h"
#include "q.h"
#include "util.h"
#include <span>
#include <stddef.h>
#include <stdint.h>

struct SensorInterfaceBus {
  virtual void begin(void) = 0;
  virtual BmErr read(uint8_t *buf, size_t len, void *arg) = 0;
  virtual BmErr write(const uint8_t *buf, size_t len, void *arg) = 0;
  virtual void end(void) = 0;
};

class LSM6DSVSensor {

private:
  SensorInterfaceBus *m_bus = nullptr;

  void set_driver_ctx(void);
  static int32_t driver_write(void *handle, uint8_t reg, const uint8_t *buf, uint16_t len);
  static int32_t driver_read(void *handle, uint8_t reg, uint8_t *buf, uint16_t len);
  static void driver_delay_ms(uint32_t ms);

protected:
  stmdev_ctx_t m_ctx;

public:
  LSM6DSVSensor(uint8_t address);
  LSM6DSVSensor(SensorInterfaceBus *bus, uint8_t address);
  LSM6DSVSensor(SensorInterfaceBus *bus);

  uint8_t m_address = 0;
  void *m_arg = nullptr;

  virtual BmErr init(void) { return BmOK; };
  virtual BmErr set_data(const uint8_t *buf, size_t len) {
    (void)buf;
    (void)len;
    return BmOK;
  }
  void set_bus(SensorInterfaceBus *bus);
};

class LSM6DSV : public LSM6DSVSensor, public SensorInterfaceBus {
public:
  BmErr init(void) override;
  void handle_interrupt(void);

  typedef struct {
    LSM6DSVSensor *sensor = nullptr;
    uint8_t reg = 0;
    uint8_t len = 0;
  } LSM6DSVSensorHub;
  BmErr start_stream(std::span<LSM6DSVSensorHub *> sensor_hub_items, size_t fifo_threshold);
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
    uint64_t ns;
    LSM6DSVAccelerometer acc;
    LSM6DSVGyro gyro;
    int16_t temp; // temperature in decidegree celsius
  } LSM6DSVReading;
  static_assert(sizeof(LSM6DSVReading) == 34);

  BmErr get_reading(LSM6DSVReading *reading);

  // Override inherited SensorInterfaceBus functions to support passthrough to sensor hub objects
  void begin() override;
  BmErr read(uint8_t *buf, size_t len, void *arg) override;
  BmErr write(const uint8_t *buf, size_t len, void *arg) override;
  void end() override;

private:
  // Ref: 9.1 of AN5922, 6 bytes of data in 256 FIFO elements
  static constexpr uint16_t MAX_UNCOMPRESSED_FIFO_BYTES = 6 * 256;
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

  struct {
    uint64_t resolution_ns = 0; // Resolution per bit in timestamp read
    uint64_t ns = 0;            // Current timestamp
    uint32_t last_count = 0;    // Last bit count in timestamp read
  } m_timestamp;

  int16_t m_temperature_dc = 0;

  bool m_sensor_hub_reg_set = false;
  uint8_t m_sensor_hub_reg = 0;
  BmSemaphore m_sensor_hub_mut;
  static constexpr uint8_t MAX_SENSOR_HUB_SENSORS = 4;
  struct {
    LSM6DSVSensor *sensor = nullptr;
    uint32_t nacks = 0;
  } m_sensor_hub[MAX_SENSOR_HUB_SENSORS];

  uint16_t m_streaming_sample_time_ms = 1000;
  BmSemaphore m_streaming_sem;

  // Scale can be adjusted and conversions will work as expected
  struct {
    lsm6dsv_xl_full_scale_t accelerometer = LSM6DSV_2g;
    lsm6dsv_gy_full_scale_t gyro = LSM6DSV_125dps;
  } m_scale;

  void begin_sensor_hub(void);
  BmErr write_sensor_hub(const uint8_t *data, size_t len, void *arg);
  BmErr read_sensor_hub(uint8_t *data, size_t len, void *arg);
  void end_sensor_hub(void);

  typedef float (*ConverterCb)(int16_t);
  static ConverterCb accelerometer_convert(lsm6dsv_xl_full_scale_t scale);
  static ConverterCb gyro_convert(lsm6dsv_gy_full_scale_t scale);
};

#endif
