#ifndef __ABSTRACT_SENSOR_INTERFACE_H__
#define __ABSTRACT_SENSOR_INTERFACE_H__

#include "lis2mdl_reg.h"
#include "lsm6dsv_reg.h"
#include "util.h"
#include <stdint.h>

struct SensorInterfaceBus {
  virtual void begin(void) = 0;
  virtual BmErr read(uint8_t *buf, size_t len, void *arg) = 0;
  virtual BmErr write(const uint8_t *buf, size_t len, void *arg) = 0;
  virtual void end(void) = 0;
};

class AbstractSensorInterface {

private:
  SensorInterfaceBus *m_bus = nullptr;

  void set_driver_ctx(void);
  static int32_t driver_write(void *handle, uint8_t reg, const uint8_t *buf, uint16_t len);
  static int32_t driver_read(void *handle, uint8_t reg, uint8_t *buf, uint16_t len);
  static void driver_delay_ms(uint32_t ms);

protected:
  // ST sensors share the same context
  stmdev_ctx_t m_ctx;

public:
  AbstractSensorInterface(uint8_t address);
  AbstractSensorInterface(SensorInterfaceBus *bus, uint8_t address);
  AbstractSensorInterface(SensorInterfaceBus *bus);

  // Address to access the sensor by if required
  uint8_t m_address = 0;

  // Argument which will be passed to SensorInterfaceBus functions
  void *m_arg = nullptr;

  virtual BmErr init(void) { return BmOK; };
  virtual BmErr set_data(const uint8_t *buf, size_t len) {
    (void)buf;
    (void)len;
    return BmOK;
  }
  void set_bus(SensorInterfaceBus *bus);
};

#endif
