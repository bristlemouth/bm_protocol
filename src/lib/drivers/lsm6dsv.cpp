#include "lsm6dsv.h"
#include "bm_config.h"

#define lsm6dsv_err_to_bm_err(f) (!f ? BmOK : BmEACCES)

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
  BmErr err = BmOK;
  LSM6DSVSensor *driver = static_cast<LSM6DSVSensor *>(handle);
  SensorInterfaceBus *bus = driver->m_bus;

  bus->begin();
  bm_err_check(err, bus->write(&reg, sizeof(reg), driver->m_arg));
  bm_err_check(err, bus->write(buf, len, driver->m_arg));
  bus->end();

  return (int32_t)err;
}

int32_t LSM6DSVSensor::driver_read(void *handle, uint8_t reg, uint8_t *buf, uint16_t len) {
  BmErr err = BmOK;
  LSM6DSVSensor *driver = static_cast<LSM6DSVSensor *>(handle);
  SensorInterfaceBus *bus = driver->m_bus;

  bus->begin();
  bm_err_check(err, bus->write(&reg, sizeof(reg), driver->m_arg));
  bm_err_check(err, bus->read(buf, len, driver->m_arg));
  bus->end();

  return (int32_t)err;
}

void LSM6DSVSensor::set_bus(SensorInterfaceBus *bus) { m_bus = bus; }

BmErr LSM6DSV::init(void) {
  m_sensor_hub_mut = bm_mutex_create();
  if (!m_sensor_hub_mut) {
    return BmENOMEM;
  }

  // Validate that the LSM6DSV device ID
  uint8_t whoami = 0;
  lsm6dsv_device_id_get(&m_ctx, &whoami);
  if (whoami != LSM6DSV_ID)
    return BmENODEV;

  // Reset the LSM6DSV
  BmErr err = lsm6dsv_err_to_bm_err(lsm6dsv_sw_por(&m_ctx));
  if (err != BmOK) {
    return err;
  }

  // Ensures 16 bit registers can't be updated until high and low registers are read
  lsm6dsv_block_data_update_set(&m_ctx, PROPERTY_ENABLE);

  // Set the accelerometer resolution, the lower (ex: 2g) the more sensitive
  lsm6dsv_xl_full_scale_set(&m_ctx, LSM6DSV_2g);

  // Configure interrupt sources for INT1
  lsm6dsv_pin_int_route_t int_route = {};
  int_route.cnt_bdr = PROPERTY_ENABLE;
  return lsm6dsv_err_to_bm_err(lsm6dsv_pin_int1_route_set(&m_ctx, &int_route));
}

BmErr LSM6DSV::start_stream(std::span<LSM6DSVSensor *> sensor_hub_items,
                            size_t fifo_threshold) {
  // Start in high performance mode
  lsm6dsv_xl_mode_set(&m_ctx, LSM6DSV_XL_HIGH_PERFORMANCE_MD);
  lsm6dsv_gy_mode_set(&m_ctx, LSM6DSV_GY_HIGH_PERFORMANCE_MD);

  // Configure sensor hub
  size_t sensor_hub_count = sensor_hub_items.size();
  if (sensor_hub_count) {

    // Initialize all sensor hub elements first
    for (auto &item : sensor_hub_items) {
      item->set_bus(this);
      item->init();
    }

    // Batch sensor hub registers to read
    lsm6dsv_sh_cfg_read_t sh_cfg_read;
    for (uint8_t i = 0; i < sensor_hub_count; i++) {
      LSM6DSVSensor *item = sensor_hub_items[i];
      sh_cfg_read.slv_add = item->m_address;
      sh_cfg_read.slv_subadd = item->m_data.reg;
      sh_cfg_read.slv_len = item->m_data.size;
      lsm6dsv_sh_slv_cfg_read(&m_ctx, i, &sh_cfg_read);
      lsm6dsv_fifo_sh_batch_slave_set(&m_ctx, i, PROPERTY_ENABLE);
      m_sensor_hub[i].sensor = item;
    }

    // Configure sensor hub sending data rate to 1.875Hz
    static constexpr lsm6dsv_sh_data_rate_t LSM6DSV_SH_1Hz875 =
        static_cast<lsm6dsv_sh_data_rate_t>(0);
    lsm6dsv_sh_data_rate_set(&m_ctx, LSM6DSV_SH_1Hz875);

    // This must be set to enable reading from device 0
    lsm6dsv_sh_write_mode_set(&m_ctx, LSM6DSV_ONLY_FIRST_CYCLE);

    lsm6dsv_sh_slave_connected_set(
        &m_ctx, static_cast<lsm6dsv_sh_slave_connected_t>(sensor_hub_count - 1));
    lsm6dsv_sh_write_mode_set(&m_ctx, LSM6DSV_ONLY_FIRST_CYCLE);
    lsm6dsv_sh_master_set(&m_ctx, PROPERTY_ENABLE);
  }

  // Set FIFO threshold
  lsm6dsv_fifo_batch_counter_threshold_set(&m_ctx, fifo_threshold);
  lsm6dsv_fifo_mode_set(&m_ctx, LSM6DSV_STREAM_MODE);

  // Set FIFO batch output data rate for accelerometer and gyro
  lsm6dsv_fifo_xl_batch_set(&m_ctx, LSM6DSV_XL_BATCHED_AT_120Hz);
  lsm6dsv_fifo_gy_batch_set(&m_ctx, LSM6DSV_GY_BATCHED_AT_120Hz);

  // Set filter settings, lp1 is only available when gyro is not in low power mode
  lsm6dsv_filt_settling_mask_t filt_settling_mask = {};
  filt_settling_mask.drdy = PROPERTY_ENABLE;
  filt_settling_mask.irq_xl = PROPERTY_ENABLE;
  filt_settling_mask.irq_g = PROPERTY_ENABLE;
  lsm6dsv_filt_settling_mask_set(&m_ctx, filt_settling_mask);
  lsm6dsv_filt_gy_lp1_set(&m_ctx, PROPERTY_ENABLE);
  lsm6dsv_filt_gy_lp1_bandwidth_set(&m_ctx, LSM6DSV_GY_MEDIUM);
  lsm6dsv_filt_xl_lp2_set(&m_ctx, PROPERTY_ENABLE);
  lsm6dsv_filt_xl_lp2_bandwidth_set(&m_ctx, LSM6DSV_XL_MEDIUM);

  // Enable timestamp collection
  lsm6dsv_fifo_timestamp_batch_set(&m_ctx, LSM6DSV_TMSTMP_DEC_1);
  lsm6dsv_timestamp_set(&m_ctx, PROPERTY_ENABLE);

  // Enable temperature collection
  lsm6dsv_fifo_temp_batch_set(&m_ctx, LSM6DSV_TEMP_BATCHED_AT_1Hz875);

  // Set Power Modes
  lsm6dsv_xl_mode_set(&m_ctx, LSM6DSV_XL_HIGH_PERFORMANCE_MD);
  lsm6dsv_gy_mode_set(&m_ctx, LSM6DSV_GY_HIGH_PERFORMANCE_MD);

  // Set data rate of accelerometer and gyro
  lsm6dsv_xl_data_rate_set(&m_ctx, LSM6DSV_ODR_AT_120Hz);
  lsm6dsv_gy_data_rate_set(&m_ctx, LSM6DSV_ODR_AT_120Hz);

  // Configure sensor hub to trigger from interrupt INT2
  lsm6dsv_sh_syncro_mode_set(&m_ctx, LSM6DSV_SH_TRIG_INT2);

  return BmOK;
}

BmErr LSM6DSV::stream_handle(void) {
  uint16_t num = 0;
  lsm6dsv_fifo_status_t fifo_status;

  // TODO: add semaphore or another blocking type situation here

  lsm6dsv_fifo_status_get(&m_ctx, &fifo_status);

  num = fifo_status.fifo_level;

  uint8_t tx_buffer[128];
  while (num--) {
    lsm6dsv_fifo_out_raw_t f_data;

    lsm6dsv_fifo_out_raw_get(&m_ctx, &f_data);
    int16_t datax = uint8_to_uint16(&f_data.data[0]);
    int16_t datay = uint8_to_uint16(&f_data.data[2]);
    int16_t dataz = uint8_to_uint16(&f_data.data[4]);
    int32_t ts = static_cast<int32_t>(uint8_to_uint32(&f_data.data[0]));
    uint8_t sensor_idx = 0;

    switch (f_data.tag) {
    case LSM6DSV_GY_NC_TAG:
      break;
    case LSM6DSV_XL_NC_TAG:
      snprintf((char *)tx_buffer, sizeof(tx_buffer), "ACC [mg]:\t%4.2f\t%4.2f\t%4.2f\r\n",
               lsm6dsv_from_fs2_to_mg(datax), lsm6dsv_from_fs2_to_mg(datay),
               lsm6dsv_from_fs2_to_mg(dataz));
      break;
    case LSM6DSV_TEMPERATURE_TAG:
      break;
    case LSM6DSV_TIMESTAMP_TAG:
      snprintf((char *)tx_buffer, sizeof(tx_buffer), "TIMESTAMP [ms] %ld\r\n", ts);
      break;
    case LSM6DSV_SENSORHUB_SLAVE0_TAG:
      //snprintf((char *)tx_buffer, sizeof(tx_buffer), "LIS2MDL [mGa]:\t%4.2f\t%4.2f\t%4.2f\r\n",
      //         lis2mdl_from_lsb_to_mgauss(*datax), lis2mdl_from_lsb_to_mgauss(*datay),
      //         lis2mdl_from_lsb_to_mgauss(*dataz));
      break;
    case LSM6DSV_SENSORHUB_SLAVE1_TAG:
      break;
    case LSM6DSV_SENSORHUB_SLAVE2_TAG:
      break;
    case LSM6DSV_SENSORHUB_SLAVE3_TAG:
      break;
    case LSM6DSV_SENSORHUB_NACK_TAG:
      // Increment nack count here
      sensor_idx = static_cast<uint8_t>(datax);
      if (sensor_idx < MAX_SENSOR_HUB_SENSORS) {
        m_sensor_hub[sensor_idx].nacks++;
      }
      break;
    default:
      break;
    }
  }

  return BmOK;
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
  lsm6dsv_xl_data_rate_set(&m_ctx, LSM6DSV_ODR_AT_240Hz);

  // Only write to the device once
  lsm6dsv_sh_write_mode_set(&m_ctx, LSM6DSV_ONLY_FIRST_CYCLE);

  lsm6dsv_sh_slave_connected_set(&m_ctx, LSM6DSV_SLV_0);
}

BmErr LSM6DSV::write_sensor_hub(const uint8_t *data, size_t len, void *arg) {
  // Always 1 byte write
  (void)len;

  // Handle first "write", ref: driver_write and driver_read for operation in LSM6DSVSensor
  if (!m_sensor_hub_reg) {
    m_sensor_hub_reg = data[0];
    return BmOK;
  }

  // Set the tx data rate high to reduce latency in this function
  lsm6dsv_sh_data_rate_set(&m_ctx, LSM6DSV_SH_240Hz);

  lsm6dsv_sh_cfg_write_t sh_cfg_write;
  sh_cfg_write.slv0_add = *static_cast<uint8_t *>(arg);
  sh_cfg_write.slv0_subadd = m_sensor_hub_reg;
  sh_cfg_write.slv0_data = *data;
  int32_t ret = lsm6dsv_sh_cfg_write(&m_ctx, &sh_cfg_write);

  // Enable I2C master mode
  lsm6dsv_sh_master_set(&m_ctx, PROPERTY_ENABLE);

  // Wait until write is performed
  lsm6dsv_status_master_t status;
  do {
    lsm6dsv_sh_status_get(&m_ctx, &status);
  } while (!status.wr_once_done);

  return !ret ? BmOK : BmEACCES;
}

BmErr LSM6DSV::read_sensor_hub(uint8_t *data, size_t len, void *arg) {
  int16_t raw_xl[3];
  int32_t ret;
  lsm6dsv_data_ready_t drdy;

  lsm6dsv_sh_cfg_read_t sh_cfg_read;
  sh_cfg_read.slv_add = *static_cast<uint8_t *>(arg);
  sh_cfg_read.slv_subadd = m_sensor_hub_reg;
  sh_cfg_read.slv_len = len;
  ret = lsm6dsv_sh_slv_cfg_read(&m_ctx, 0, &sh_cfg_read);

  // Enable I2C master mode
  lsm6dsv_sh_master_set(&m_ctx, PROPERTY_ENABLE);

  // Wait accelerometer operation flag set
  lsm6dsv_acceleration_raw_get(&m_ctx, raw_xl);
  do {
    lsm6dsv_flag_data_ready_get(&m_ctx, &drdy);
  } while (!drdy.drdy_xl);

  // Wait for sensor hub operation flag set
  lsm6dsv_status_master_t status;
  do {
    lsm6dsv_sh_status_get(&m_ctx, &status);
  } while (!status.sens_hub_endop);

  // Read SensorHub registers
  lsm6dsv_sh_read_data_raw_get(&m_ctx, data, len);

  return !ret ? BmOK : BmEACCES;
}

void LSM6DSV::end_sensor_hub(void) {
  // Disable I2C master
  lsm6dsv_sh_master_set(&m_ctx, PROPERTY_DISABLE);

  // Reset sensor hub address
  m_sensor_hub_reg = 0;

  bm_semaphore_give(m_sensor_hub_mut);
}
