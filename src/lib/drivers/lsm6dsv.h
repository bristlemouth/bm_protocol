#ifndef __LSM6DSV_H__
#define __LSM6DSV_H__

#include "bm_os.h"
#include "lsm6dsv_reg.h"
#include "util.h"
#include <span>
#include <stddef.h>
#include <stdint.h>

struct SensorInterfaceBus {
  virtual void begin() = 0;
  virtual BmErr read(uint8_t *buf, size_t len, void *arg) = 0;
  virtual BmErr write(const uint8_t *buf, size_t len, void *arg) = 0;
  virtual void end() = 0;
};

class LSM6DSVSensor {

private:
  SensorInterfaceBus *m_bus = nullptr;
  void *m_arg = nullptr;

  void set_driver_ctx(void);
  static int32_t driver_write(void *handle, uint8_t reg, const uint8_t *buf, uint16_t len);
  static int32_t driver_read(void *handle, uint8_t reg, uint8_t *buf, uint16_t len);
  static void driver_delay_ms(uint32_t ms);

protected:
  stmdev_ctx_t m_ctx;

public:
  struct {
    uint16_t reg = 0;
    uint8_t size = 0;
  } m_data;

  LSM6DSVSensor(uint8_t address);
  LSM6DSVSensor(SensorInterfaceBus *bus, uint8_t address);
  LSM6DSVSensor(SensorInterfaceBus *bus);

  uint8_t m_address = 0;

  virtual BmErr init(void);
  void set_bus(SensorInterfaceBus *bus);
};

class LSM6DSV : public LSM6DSVSensor, public SensorInterfaceBus {
public:
  BmErr init(void) override;
  BmErr start_stream(std::span<LSM6DSVSensor *> sensor_hub_items, size_t fifo_threshold);
  BmErr stream_handle(void);

  // Override inherited SensorInterfaceBus functions to support passthrough to sensor hub objects
  void begin() override;
  BmErr read(uint8_t *buf, size_t len, void *arg) override;
  BmErr write(const uint8_t *buf, size_t len, void *arg) override;
  void end() override;

private:
  uint8_t m_sensor_hub_reg = 0;
  BmSemaphore m_sensor_hub_mut;
  static constexpr uint8_t MAX_SENSOR_HUB_SENSORS = 4;
  struct {
    LSM6DSVSensor *sensor = nullptr;
    uint32_t nacks = 0;
  } m_sensor_hub[MAX_SENSOR_HUB_SENSORS];

  void begin_sensor_hub(void);
  BmErr write_sensor_hub(const uint8_t *data, size_t len, void *arg);
  BmErr read_sensor_hub(uint8_t *data, size_t len, void *arg);
  void end_sensor_hub(void);
};

#endif
