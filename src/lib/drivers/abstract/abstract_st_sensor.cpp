#include "abstract_st_sensor.h"
#include "bm_config.h"
#include "bm_os.h"

void AbstractSensorInterface::set_driver_ctx(void) {
  // Set the read and write functions for the driver
  m_ctx.write_reg = driver_write;
  m_ctx.read_reg = driver_read;
  m_ctx.mdelay = driver_delay_ms;
  m_ctx.handle = this;
}

AbstractSensorInterface::AbstractSensorInterface(uint8_t address) : m_address(address) {
  set_driver_ctx();
}

AbstractSensorInterface::AbstractSensorInterface(SensorInterfaceBus *bus, uint8_t address)
    : m_bus(bus), m_address(address) {
  set_driver_ctx();
}

AbstractSensorInterface::AbstractSensorInterface(SensorInterfaceBus *bus) : m_bus(bus) {
  set_driver_ctx();
}

void AbstractSensorInterface::driver_delay_ms(uint32_t ms) { bm_delay(ms); }

int32_t AbstractSensorInterface::driver_write(void *handle, uint8_t reg, const uint8_t *buf,
                                              uint16_t len) {
  AbstractSensorInterface *driver = static_cast<AbstractSensorInterface *>(handle);
  SensorInterfaceBus *bus = driver->m_bus;
  if (!bus) {
    return BmENODEV;
  }

  BmErr err = BmOK;
  bus->begin();
  bm_err_check(err, bus->write(&reg, sizeof(reg), driver->m_arg));
  bm_err_check(err, bus->write(buf, len, driver->m_arg));
  bus->end();

  return (int32_t)err;
}

int32_t AbstractSensorInterface::driver_read(void *handle, uint8_t reg, uint8_t *buf,
                                             uint16_t len) {
  AbstractSensorInterface *driver = static_cast<AbstractSensorInterface *>(handle);
  SensorInterfaceBus *bus = driver->m_bus;
  if (!bus) {
    return BmENODEV;
  }

  BmErr err = BmOK;
  bus->begin();
  bm_err_check(err, bus->write(&reg, sizeof(reg), driver->m_arg));
  bm_err_check(err, bus->read(buf, len, driver->m_arg));
  bus->end();

  return (int32_t)err;
}

/*!
 @brief Set the SensorInterfaceBus for the ST sensor

 @details The sensor interface bus can be set after the constructor. If the bus
          is NULL, the device will be inaccessible.

 @param bus bus to assign to the sensor
 */
void AbstractSensorInterface::set_bus(SensorInterfaceBus *bus) { m_bus = bus; }
