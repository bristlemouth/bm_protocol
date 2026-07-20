#include "xld.h"
#include <string.h>

XLD::XLD(SensorInterfaceBus *bus, void *arg) : m_bus(bus), m_arg(arg) {}

XLD::XLD(SensorInterfaceBus *bus, void *arg, uint8_t address)
    : m_address(address), m_bus(bus), m_arg(arg) {}

/*!
 @brief Initialize Keller sensor

 @details Will perform the following:
            - Read the product ID of the device to see if it is reachable
            - Calculate the Pmin and Pmax values to determine reading scaling
            - Setup semaphore to determine when readings are ready

 @return BmEINVAL if no I2C insatnce
         BmENODEV if failed to read device or product ID is not expected
         BmENOMEM if semaphore cannot be created
         Other error based on write/read bus implementation
 */
BmErr XLD::init(void) {
  if (!m_bus) {
    return BmEINVAL;
  }

  // Ensure device is reachable
  uint32_t prod_id;
  BmErr err = get_prod_id(&prod_id);
  if (err != BmOK) {
    return BmENODEV;
  }

  // Calculate Pmin and Pmax
  err = calc_lim(SCALING1, &m_pmin);
  bm_err_check(err, calc_lim(SCALING3, &m_pmax));

  if (err == BmOK) {
    m_data_ready_sem = bm_semaphore_create();
    if (!m_data_ready_sem) {
      return BmENOMEM;
    }
  }

  return err;
}

/*!
 @brief Handle EOC interrupt

 @details Must be called in the interrupt handler tied to the EOC pin.
          Indicates when a reading has been performed. Pin is active low.
 */
void XLD::handle_interrupt(void) {
  if (m_data_ready_sem) {
    bm_semaphore_give(m_data_ready_sem);
  }
}

/*!
 @brief Request a reading from the sensor

 @details The reading will take about 8ms to perform. A ready status is
          indicated by the EOC pin.

 @return BmENODEV if there is no device connected
         Other error based on write bus implementation
 */
BmErr XLD::request_reading(void) {
  if (!m_data_ready_sem) {
    return BmENODEV;
  }

  return send_command(GET_MEASURMENT);
}

/*!
 @brief Get reading from sensor if ready

 @param mbar optional pressure value to populate from reading in millibar
 @param temp optional temperature value to populate from reading in celsius
 @param timeout_ms timeout to wait for reading

 @return BmENODEV if there is no device connected
         BmETIMEDOUT if waiting on semaphore times out
         BmEIO if there is a memory error, must reset device if so
         BmEACCES if the device is still busy
         Other error based on write/read bus implementation
 */
BmErr XLD::get_reading(float *mbar, float *temp, uint32_t timeout_ms) {
  if (!m_data_ready_sem) {
    return BmENODEV;
  }

  BmErr err = bm_semaphore_take(m_data_ready_sem, timeout_ms);
  if (err != BmOK) {
    return err;
  }

  XLDStatus status = {};
  uint8_t buf[sizeof(uint32_t)] = {};
  err = read_device(&status, buf, sizeof(buf));
  if (err != BmOK) {
    return err;
  } else if (status.memory_err) {
    return BmEIO;
  } else if (status.busy) {
    return BmEACCES;
  }

  uint16_t reading;
  if (mbar) {
    reading = uint8_to_uint16(buf);
    *mbar = (static_cast<float>(reading) - 16384.0f) * (m_pmax - m_pmin) / 32768.0f + m_pmin;
    *mbar *= 1000.0f;
  }

  if (temp) {
    reading = uint8_to_uint16(&buf[2]);
    *temp = ((static_cast<float>(reading >> 4) - 24.0f) * 0.05f) - 50.0f;
  }

  return BmOK;
}

BmErr XLD::get_prod_id(uint32_t *id) {
  uint16_t read_0;
  uint16_t read_1;
  BmErr err = read_memory_map(CUST_ID0, &read_0);
  bm_err_check(err, read_memory_map(CUST_ID1, &read_1));

  if (err == BmOK && id) {
    *id = convert_u32(read_1, read_0);
  }

  return err;
}

BmErr XLD::calc_lim(XLDCmd initial, float *lim) {
  uint16_t p_lower;
  uint16_t p_upper;

  BmErr err = read_memory_map(initial, &p_upper);
  initial = static_cast<XLDCmd>(initial + 1);
  bm_err_check(err, read_memory_map(initial, &p_lower));
  if (err == BmOK) {
    uint32_t bits = convert_u32(p_upper, p_lower);

    // Direct bit-reinterpreting cast, stored as float on device
    memcpy(lim, &bits, sizeof(bits));
  }

  return err;
}

BmErr XLD::read_memory_map(XLDCmd cmd, uint16_t *data) {
  static constexpr uint8_t memory_map_delay_ms = 1;

  BmErr err = send_command(cmd);
  if (err != BmOK) {
    return err;
  }

  bm_delay(memory_map_delay_ms);

  uint8_t buf[sizeof(uint16_t)] = {};
  err = read_device(NULL, buf, sizeof(buf));
  if (err != BmOK) {
    return err;
  }

  *data = uint8_to_uint16(buf);

  return BmOK;
}

BmErr XLD::send_command(XLDCmd cmd) {
  uint8_t buf = static_cast<uint8_t>(cmd);
  m_bus->begin();
  BmErr err = m_bus->write(m_address, &buf, sizeof(buf), m_arg);
  m_bus->end();

  return err;
}

BmErr XLD::send_command(XLDCmd cmd, uint8_t *data, uint8_t size) {
  uint8_t buf_size = size + sizeof(XLDCmd);
  uint8_t buf[buf_size];
  buf[0] = cmd;
  memcpy(&buf[1], data, size);
  m_bus->begin();
  BmErr err = m_bus->write(m_address, buf, buf_size, m_arg);
  m_bus->end();

  return err;
}

BmErr XLD::read_device(XLDStatus *status, uint8_t *data, uint8_t size) {
  uint8_t buf_size = size + sizeof(XLDStatus);
  uint8_t buf[buf_size];
  m_bus->begin();
  BmErr err = m_bus->read(m_address, buf, buf_size, m_arg);
  m_bus->end();
  if (err != BmOK) {
    return err;
  }

  if (status) {
    memcpy(status, buf, sizeof(XLDStatus));
  }

  if (data) {
    memcpy(data, &buf[1], size);
  }

  return BmOK;
}
