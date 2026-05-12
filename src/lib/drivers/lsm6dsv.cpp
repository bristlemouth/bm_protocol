#include "lsm6dsv.h"
#include "FreeRTOS.h"
#include "app_util.h"
#include "bm_config.h"
#include "semphr.h"
#include "uptime.h"

#define lsm6dsv_return_on_err(f)                                                               \
  ({                                                                                           \
    int32_t err = (f);                                                                         \
    if (err != 0) {                                                                            \
      bm_debug("lsm6dsv err %d:%" PRId32 "\n", __LINE__, err);                                 \
      return !err ? BmOK : BmEACCES;                                                           \
    }                                                                                          \
  })
#define poll_bit(f, reg, bit)                                                                  \
  ({                                                                                           \
    reg status = {};                                                                           \
    static constexpr uint16_t timeout_ms = 2 * 1000 / 120;                                     \
    uint32_t time_start_ms = uptimeGetMs();                                                    \
    uint32_t time_diff_ms;                                                                     \
    do {                                                                                       \
      f(&m_ctx, &status);                                                                      \
      time_diff_ms = uptimeGetMs() - time_start_ms;                                            \
    } while (!status.bit && time_diff_ms < timeout_ms);                                        \
    status.bit ? BmOK : BmETIMEDOUT;                                                           \
  })

void LSM6DSVSensor::set_driver_ctx(void) {
  // Set the read and write functions for the driver
  m_ctx.write_reg = driver_write;
  m_ctx.read_reg = driver_read;
  m_ctx.mdelay = driver_delay_ms;
  m_ctx.handle = this;
}

LSM6DSVSensor::LSM6DSVSensor(uint8_t address) : m_address(address) { set_driver_ctx(); }

LSM6DSVSensor::LSM6DSVSensor(SensorInterfaceBus *bus, uint8_t address)
    : m_bus(bus), m_address(address) {
  set_driver_ctx();
}

LSM6DSVSensor::LSM6DSVSensor(SensorInterfaceBus *bus) : m_bus(bus) { set_driver_ctx(); }

void LSM6DSVSensor::driver_delay_ms(uint32_t ms) { bm_delay(ms); }

int32_t LSM6DSVSensor::driver_write(void *handle, uint8_t reg, const uint8_t *buf,
                                    uint16_t len) {
  LSM6DSVSensor *driver = static_cast<LSM6DSVSensor *>(handle);
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

int32_t LSM6DSVSensor::driver_read(void *handle, uint8_t reg, uint8_t *buf, uint16_t len) {
  LSM6DSVSensor *driver = static_cast<LSM6DSVSensor *>(handle);
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

void LSM6DSVSensor::set_bus(SensorInterfaceBus *bus) { m_bus = bus; }

BmErr LSM6DSV::init(void) {
  BmErr err = q_create_static(&m_reading_queue, m_readings_buf, sizeof(m_readings_buf));
  if (err != BmOK) {
    return err;
  }

  // Validate the LSM6DSV device ID
  uint8_t whoami = 0;
  lsm6dsv_return_on_err(lsm6dsv_device_id_get(&m_ctx, &whoami));
  if (whoami != LSM6DSV_ID) {
    return BmENODEV;
  }

  // Reset the LSM6DSV
  lsm6dsv_return_on_err(lsm6dsv_sw_por(&m_ctx));

  // Ensures 16 bit registers can't be updated until high and low registers are read
  lsm6dsv_return_on_err(lsm6dsv_block_data_update_set(&m_ctx, PROPERTY_ENABLE));

  // Set the accelerometer resolution, the lower (ex: 2g) the more sensitive
  lsm6dsv_return_on_err(lsm6dsv_xl_full_scale_set(&m_ctx, m_scale.accelerometer));

  // Set the gyro resolution
  lsm6dsv_return_on_err(lsm6dsv_gy_full_scale_set(&m_ctx, m_scale.gyro));

  // Calculate sampling timestamp resolution t = 1 / (46080 * (1 + 0.0013 * FREQ_FINE), ref: 6.4 AN5922
  int8_t freq_fine;
  lsm6dsv_return_on_err(lsm6dsv_odr_cal_reg_get(&m_ctx, &freq_fine));
  uint64_t denominator = 46080000 + 59904 * static_cast<uint64_t>(freq_fine);
  uint64_t numerator = 1e9 * 1000;
  m_timestamp.resolution_ns = numerator / denominator;

  // Configure interrupt sources for INT1
  lsm6dsv_pin_int_route_t int_route = {};
  int_route.cnt_bdr = PROPERTY_ENABLE;
  lsm6dsv_return_on_err(lsm6dsv_pin_int1_route_set(&m_ctx, &int_route));

  m_sensor_hub_mut = bm_mutex_create();
  m_queue_mut = bm_mutex_create();
  m_streaming_sem = bm_semaphore_create();
  if (!m_sensor_hub_mut || !m_queue_mut || !m_streaming_sem) {
    bm_free(m_sensor_hub_mut);
    bm_free(m_queue_mut);
    bm_free(m_streaming_sem);
    return BmENOMEM;
  }

  return BmOK;
}

void LSM6DSV::handle_interrupt(void) {
  BaseType_t task_yield;

  xSemaphoreGiveFromISR(m_streaming_sem, &task_yield);
  portYIELD_FROM_ISR(task_yield);
}

BmErr LSM6DSV::start_stream(std::span<LSM6DSVSensorHub *> sensor_hub_items,
                            size_t fifo_threshold) {
  // Start in high performance mode
  lsm6dsv_return_on_err(lsm6dsv_xl_mode_set(&m_ctx, LSM6DSV_XL_HIGH_PERFORMANCE_MD));
  lsm6dsv_return_on_err(lsm6dsv_gy_mode_set(&m_ctx, LSM6DSV_GY_HIGH_PERFORMANCE_MD));

  // Configure sensor hub
  size_t sensor_hub_count = sensor_hub_items.size();
  if (sensor_hub_count > MAX_SENSOR_HUB_SENSORS) {
    return BmEINVAL;
  }

  if (sensor_hub_count) {

    // Initialize all sensor hub elements first
    for (auto &item : sensor_hub_items) {
      // Set the argument used to point to the address of the device
      LSM6DSVSensor *sensor = item->sensor;
      sensor->m_arg = &sensor->m_address;
      sensor->set_bus(this);
      sensor->init();
    }

    // Batch sensor hub registers to read
    lsm6dsv_sh_cfg_read_t sh_cfg_read;
    for (uint8_t i = 0; i < sensor_hub_count; i++) {
      LSM6DSVSensorHub *item = sensor_hub_items[i];
      sh_cfg_read.slv_add = item->sensor->m_address;
      sh_cfg_read.slv_subadd = item->reg;
      sh_cfg_read.slv_len = item->len;
      lsm6dsv_return_on_err(lsm6dsv_sh_slv_cfg_read(&m_ctx, i, &sh_cfg_read));
      lsm6dsv_return_on_err(lsm6dsv_fifo_sh_batch_slave_set(&m_ctx, i, PROPERTY_ENABLE));
      m_sensor_hub[i].sensor = item->sensor;
    }

    // Configure sensor hub batch data rate if triggered by accelerometer or gyro
    lsm6dsv_return_on_err(lsm6dsv_sh_data_rate_set(&m_ctx, LSM6DSV_SH_120Hz));

    // This must be set to enable reading from device 0
    lsm6dsv_return_on_err(lsm6dsv_sh_write_mode_set(&m_ctx, LSM6DSV_ONLY_FIRST_CYCLE));

    lsm6dsv_return_on_err(lsm6dsv_sh_slave_connected_set(
        &m_ctx, static_cast<lsm6dsv_sh_slave_connected_t>(sensor_hub_count - 1)));
    lsm6dsv_return_on_err(lsm6dsv_sh_master_set(&m_ctx, PROPERTY_ENABLE));
  }

  // Set FIFO threshold
  lsm6dsv_return_on_err(lsm6dsv_fifo_batch_counter_threshold_set(&m_ctx, fifo_threshold));
  lsm6dsv_return_on_err(lsm6dsv_fifo_mode_set(&m_ctx, LSM6DSV_STREAM_MODE));

  // Set FIFO batch output data rate for accelerometer and gyro
  lsm6dsv_return_on_err(lsm6dsv_fifo_xl_batch_set(&m_ctx, LSM6DSV_XL_BATCHED_AT_120Hz));
  lsm6dsv_return_on_err(lsm6dsv_fifo_gy_batch_set(&m_ctx, LSM6DSV_GY_BATCHED_AT_120Hz));

  // Set filter settings, lp1 is only available when gyro is not in low power mode
  lsm6dsv_filt_settling_mask_t filt_settling_mask = {};
  filt_settling_mask.drdy = PROPERTY_ENABLE;
  filt_settling_mask.irq_xl = PROPERTY_ENABLE;
  filt_settling_mask.irq_g = PROPERTY_ENABLE;
  lsm6dsv_return_on_err(lsm6dsv_filt_settling_mask_set(&m_ctx, filt_settling_mask));
  lsm6dsv_return_on_err(lsm6dsv_filt_gy_lp1_set(&m_ctx, PROPERTY_ENABLE));
  lsm6dsv_return_on_err(lsm6dsv_filt_gy_lp1_bandwidth_set(&m_ctx, LSM6DSV_GY_MEDIUM));
  lsm6dsv_return_on_err(lsm6dsv_filt_xl_lp2_set(&m_ctx, PROPERTY_ENABLE));
  lsm6dsv_return_on_err(lsm6dsv_filt_xl_lp2_bandwidth_set(&m_ctx, LSM6DSV_XL_MEDIUM));

  // Enable timestamp collection
  lsm6dsv_return_on_err(lsm6dsv_fifo_timestamp_batch_set(&m_ctx, LSM6DSV_TMSTMP_DEC_1));
  lsm6dsv_return_on_err(lsm6dsv_timestamp_set(&m_ctx, PROPERTY_ENABLE));

  // Enable temperature collection and obtain initial temperature
  lsm6dsv_return_on_err(lsm6dsv_fifo_temp_batch_set(&m_ctx, LSM6DSV_TEMP_BATCHED_AT_1Hz875));
  int16_t reg;
  lsm6dsv_return_on_err(lsm6dsv_temperature_raw_get(&m_ctx, &reg));
  m_temperature_dc = static_cast<int16_t>(lsm6dsv_from_lsb_to_celsius(reg) * 10.0);

  // Set Power Modes
  lsm6dsv_return_on_err(lsm6dsv_xl_mode_set(&m_ctx, LSM6DSV_XL_HIGH_PERFORMANCE_MD));
  lsm6dsv_return_on_err(lsm6dsv_gy_mode_set(&m_ctx, LSM6DSV_GY_HIGH_PERFORMANCE_MD));

  // Set data rate of accelerometer and gyro
  lsm6dsv_return_on_err(lsm6dsv_xl_data_rate_set(&m_ctx, LSM6DSV_ODR_AT_120Hz));
  lsm6dsv_return_on_err(lsm6dsv_gy_data_rate_set(&m_ctx, LSM6DSV_ODR_AT_120Hz));

  // Configure sensor hub to trigger from interrupt INT2
  lsm6dsv_return_on_err(lsm6dsv_sh_syncro_mode_set(&m_ctx, LSM6DSV_SH_TRIG_INT2));

  return BmOK;
}

LSM6DSV::ConverterCb LSM6DSV::accelerometer_convert(lsm6dsv_xl_full_scale_t scale) {
  switch (scale) {
  case LSM6DSV_2g:
  default:
    return lsm6dsv_from_fs2_to_mg;
  case LSM6DSV_4g:
    return lsm6dsv_from_fs4_to_mg;
  case LSM6DSV_8g:
    return lsm6dsv_from_fs8_to_mg;
  case LSM6DSV_16g:
    return lsm6dsv_from_fs16_to_mg;
  }
}

LSM6DSV::ConverterCb LSM6DSV::gyro_convert(lsm6dsv_gy_full_scale_t scale) {
  switch (scale) {
  case LSM6DSV_125dps:
  default:
    return lsm6dsv_from_fs125_to_mdps;
  case LSM6DSV_250dps:
    return lsm6dsv_from_fs250_to_mdps;
  case LSM6DSV_500dps:
    return lsm6dsv_from_fs500_to_mdps;
  case LSM6DSV_1000dps:
    return lsm6dsv_from_fs1000_to_mdps;
  case LSM6DSV_2000dps:
    return lsm6dsv_from_fs2000_to_mdps;
  case LSM6DSV_4000dps:
    return lsm6dsv_from_fs4000_to_mdps;
  }
}

BmErr LSM6DSV::stream_handle(void) {
  uint16_t num = 0;
  lsm6dsv_fifo_status_t fifo_status;

  bm_semaphore_take(m_streaming_sem, m_streaming_sample_time_ms);

  lsm6dsv_return_on_err(lsm6dsv_fifo_status_get(&m_ctx, &fifo_status));

  num = fifo_status.fifo_level;
  LSM6DSVReading reading = {};

  static const uint8_t samples_per_readings = 3;
  uint8_t samples_set = 0;

  while (num--) {
    lsm6dsv_fifo_out_raw_t f_data;

    lsm6dsv_return_on_err(lsm6dsv_fifo_out_raw_get(&m_ctx, &f_data));
    int16_t datax = le_uint8_to_uint16(&f_data.data[0]);
    int16_t datay = le_uint8_to_uint16(&f_data.data[2]);
    int16_t dataz = le_uint8_to_uint16(&f_data.data[4]);
    uint32_t ts;
    uint8_t sensor_nack_idx;
    ConverterCb cb;

    switch (f_data.tag) {
    case LSM6DSV_GY_NC_TAG:
      cb = gyro_convert(m_scale.gyro);
      reading.gyro.x = cb(datax);
      reading.gyro.y = cb(datay);
      reading.gyro.z = cb(dataz);
      samples_set++;
      break;
    case LSM6DSV_XL_NC_TAG:
      cb = accelerometer_convert(m_scale.accelerometer);
      reading.acc.x = cb(datax);
      reading.acc.y = cb(datay);
      reading.acc.z = cb(dataz);
      samples_set++;
      break;
    case LSM6DSV_TEMPERATURE_TAG:
      m_temperature_dc = static_cast<int16_t>(lsm6dsv_from_lsb_to_celsius(datax) * 10.0);
      break;
    case LSM6DSV_TIMESTAMP_TAG:
      // Timestamp register is typically 21.7uS per bit, but the precise
      // measurement is calculated with m_timestamp.resolution_ns,
      // ref: 6.4 AN5922
      ts = le_uint8_to_uint32(&f_data.data[0]);

      // Account for rollover here by examining the last timestamp count
      m_timestamp.ns += (ts - m_timestamp.last_count) * m_timestamp.resolution_ns;
      m_timestamp.last_count = ts;

      reading.ns = m_timestamp.ns;
      samples_set++;
      break;
    case LSM6DSV_SENSORHUB_NACK_TAG:
      // Increment nack count here, ref: table 92 of AN5922
      sensor_nack_idx = static_cast<uint8_t>(datax);
      if (sensor_nack_idx < MAX_SENSOR_HUB_SENSORS) {
        m_sensor_hub[sensor_nack_idx].nacks++;
      }
      break;
    default:
      break;
    }

    // If all information is collected, queue the data to be handled
    if (samples_set >= samples_per_readings) {
      // Use latest temperature value
      reading.temp = m_temperature_dc;

      bm_semaphore_take(m_queue_mut, BM_MAX_DELAY_UINT32);
      q_enqueue(&m_reading_queue, &reading, sizeof(LSM6DSVReading));
      bm_semaphore_give(m_queue_mut);
      samples_set = 0;
      reading = {};
    }

    // Set sensor hub data, ref: 9.6.1 of AN5922
    if (f_data.tag >= LSM6DSV_SENSORHUB_SLAVE0_TAG &&
        f_data.tag <= LSM6DSV_SENSORHUB_SLAVE3_TAG) {
      uint8_t sensor_set_idx = f_data.tag - LSM6DSV_SENSORHUB_SLAVE0_TAG;
      LSM6DSVSensor *sensor = m_sensor_hub[sensor_set_idx].sensor;
      if (sensor) {
        sensor->set_data(f_data.data, sizeof(f_data.data));
      }
    }
  }

  return BmOK;
}

BmErr LSM6DSV::get_reading(LSM6DSVReading *reading) {
  if (!reading) {
    return BmEINVAL;
  }

  bm_semaphore_take(m_queue_mut, BM_MAX_DELAY_UINT32);
  BmErr err = q_dequeue(&m_reading_queue, reading, sizeof(LSM6DSVReading));
  bm_semaphore_give(m_queue_mut);

  return err;
}

void LSM6DSV::begin() { begin_sensor_hub(); }

BmErr LSM6DSV::read(uint8_t *buf, size_t len, void *arg) {
  return read_sensor_hub(buf, len, arg);
}
BmErr LSM6DSV::write(const uint8_t *buf, size_t len, void *arg) {
  return write_sensor_hub(buf, len, arg);
}

void LSM6DSV::end() { end_sensor_hub(); }

void LSM6DSV::begin_sensor_hub(void) {
  bm_semaphore_take(m_sensor_hub_mut, BM_MAX_DELAY_UINT32);

  // Enable accelerometer to trigger Sensor Hub operation, high data rate to reduce latency
  lsm6dsv_xl_data_rate_set(&m_ctx, LSM6DSV_ODR_AT_120Hz);

  // Only write to the device once
  lsm6dsv_sh_write_mode_set(&m_ctx, LSM6DSV_ONLY_FIRST_CYCLE);

  lsm6dsv_sh_slave_connected_set(&m_ctx, LSM6DSV_SLV_0);
}

BmErr LSM6DSV::write_sensor_hub(const uint8_t *data, size_t len, void *arg) {
  // Always 1 byte write
  (void)len;

  // Handle first "write", ref: driver_write and driver_read for operation in LSM6DSVSensor
  if (!m_sensor_hub_reg_set) {
    m_sensor_hub_reg = data[0];
    m_sensor_hub_reg_set = true;
    return BmOK;
  }

  // Set the tx data rate high to reduce latency in this function
  lsm6dsv_return_on_err(lsm6dsv_sh_data_rate_set(&m_ctx, LSM6DSV_SH_120Hz));

  lsm6dsv_sh_cfg_write_t sh_cfg_write;
  sh_cfg_write.slv0_add = *static_cast<uint8_t *>(arg);
  sh_cfg_write.slv0_subadd = m_sensor_hub_reg;
  sh_cfg_write.slv0_data = *data;
  lsm6dsv_return_on_err(lsm6dsv_sh_cfg_write(&m_ctx, &sh_cfg_write));

  // Enable I2C master mode
  lsm6dsv_return_on_err(lsm6dsv_sh_master_set(&m_ctx, PROPERTY_ENABLE));

  // Wait until write is performed
  return poll_bit(lsm6dsv_sh_status_get, lsm6dsv_status_master_t, wr_once_done);
}

BmErr LSM6DSV::read_sensor_hub(uint8_t *data, size_t len, void *arg) {
  int16_t raw_xl[3];
  int32_t ret;

  lsm6dsv_sh_cfg_read_t sh_cfg_read;
  sh_cfg_read.slv_add = *static_cast<uint8_t *>(arg);
  sh_cfg_read.slv_subadd = m_sensor_hub_reg;
  sh_cfg_read.slv_len = len;
  lsm6dsv_return_on_err(lsm6dsv_sh_slv_cfg_read(&m_ctx, 0, &sh_cfg_read));

  // Enable I2C master mode
  lsm6dsv_return_on_err(lsm6dsv_sh_master_set(&m_ctx, PROPERTY_ENABLE));

  // Wait accelerometer operation flag set
  lsm6dsv_return_on_err(lsm6dsv_acceleration_raw_get(&m_ctx, raw_xl));
  ret = poll_bit(lsm6dsv_flag_data_ready_get, lsm6dsv_data_ready_t, drdy_xl);
  if (ret) {
    return static_cast<BmErr>(ret);
  }

  // Wait for sensor hub operation flag set
  ret = poll_bit(lsm6dsv_sh_status_get, lsm6dsv_status_master_t, sens_hub_endop);
  if (ret) {
    return static_cast<BmErr>(ret);
  }

  // Read SensorHub registers
  lsm6dsv_return_on_err(lsm6dsv_sh_read_data_raw_get(&m_ctx, data, len));

  return BmOK;
}

void LSM6DSV::end_sensor_hub(void) {
  // Reset sensor hub address
  m_sensor_hub_reg_set = false;

  // Disable I2C master
  lsm6dsv_sh_master_set(&m_ctx, PROPERTY_DISABLE);

  bm_semaphore_give(m_sensor_hub_mut);
}
