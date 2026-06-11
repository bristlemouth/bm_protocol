#include "lsm6dsv.h"
#include "FreeRTOS.h"
#include "app_util.h"
#include "bm_config.h"
#include "semphr.h"
#include "uptime.h"
#include <cstring>

#define lsm6dsv_return_on_err(f)                                                               \
  ({                                                                                           \
    int32_t err = (f);                                                                         \
    if (err != 0) {                                                                            \
      bm_debug("lsm6dsv err %d:%" PRId32 "\n", __LINE__, err);                                 \
      return BmEACCES;                                                                         \
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

/*!
 @brief Initialize LSM6DSV IMU

 @details This function performs the following:
            - Creates a queue to hold LSM6DSVReading data
            - Ensures the imu exists and can be recognized on the bus
            - Performs a software reset of the imu
            - Calculates the precise sampling period in nanoseconds
            - Configures the BDR as the interrupt source
            - Creates mutexes and a semaphore for protecting resources
          The LSM6DSV is configured to use the FIFO functionality, allowing
          readings to buffer on the imu before offloading them to be handled
          by the processor.

 @param cfg Pointer to a configuration to use for driver, if NULL use default

 @return BmOK on success
         BmENOMEM if there is insufficient memory to create mutexes/semaphore
         BmENODEV if the imu is not found
         BmEACCES if bus operations fail when attempting communication to imu
 */
BmErr LSM6DSV::init(const Cfg *cfg) {
  if (cfg) {
    m_cfg = *cfg;
  }

  // Do not support high accuracy data rates ref: 6.5 DS13476
  // Gyro cannot support 1.875Hz rate ref: Table 54 DS13476
  if (m_cfg.sample_rate > LSM6DSV_ODR_AT_7680Hz || cfg->sample_rate == LSM6DSV_ODR_AT_1Hz875) {
    return BmEINVAL;
  }

  BmErr err = q_create_static(&m_reading_queue, m_readings_buf, sizeof(m_readings_buf));
  if (err != BmOK) {
    return err;
  }

  // Wait 10ms for calibration data to be stored in internal registers, ref 4.1 AN5922
  static constexpr uint8_t cal_delay_ms = 10;
  bm_delay(cal_delay_ms);

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

  // Calculate sampling timestamp resolution t = 1 / (46080 * (1 + 0.0013 * FREQ_FINE), ref: 6.4 AN5922
  int8_t freq_fine;
  lsm6dsv_return_on_err(lsm6dsv_odr_cal_reg_get(&m_ctx, &freq_fine));
  uint64_t denominator = 46080000 + 59904 * static_cast<uint64_t>(freq_fine);
  static constexpr uint64_t numerator = 1000000000ULL * 1000;
  m_timestamp.resolution_ns = numerator / denominator;

  // Configure interrupt sources for INT1
  lsm6dsv_pin_int_route_t int_route = {};
  int_route.cnt_bdr = PROPERTY_ENABLE;
  lsm6dsv_return_on_err(lsm6dsv_pin_int1_route_set(&m_ctx, &int_route));

  m_sensor_hub_mut = bm_mutex_create();
  m_queue_mut = bm_mutex_create();
  m_streaming_sem = bm_semaphore_create();
  m_reading_sem = bm_semaphore_create();
  if (!m_sensor_hub_mut || !m_queue_mut || !m_streaming_sem || !m_reading_sem) {
    bm_free(m_sensor_hub_mut);
    bm_free(m_queue_mut);
    bm_free(m_streaming_sem);
    bm_free(m_reading_sem);
    return BmENOMEM;
  }

  return BmOK;
}

/*!
 @brief Handle interrupt from LSM6DSV

 @details This function is safe to call from interrupt context. Informs the
          stream_handle function that data is available. Note the interrupt
          pin is configured as active high.

 @see stream_handle
 */
void LSM6DSV::handle_interrupt(void) {
  BaseType_t task_yield;

  xSemaphoreGiveFromISR(m_streaming_sem, &task_yield);
  portYIELD_FROM_ISR(task_yield);
}

/*!
 @brief Configure the LSM6DSV and begin data collection

 @details The following configuration is done for the LSM6DSV:
            - Initializes and adds sensor hub sensors to perform readings
            - Configures the resolution of the gyro and the accelerometer
            - Configure low pass filter settings for gyro and accelerometer
            - Configure timestamp collection per gyro and accelerometer reading
            - Configure temperature readings to be obtained from the FIFO
            - Sets power modes for gyro and accelerometer

 @param sensor_hub_items array of sensors to be initialized and read from the
                         sensor hub functionality
 @param fifo_threshold number of items in the FIFO before an interrupt is
                       triggered
 @param sensor_hub_poll should the sensor hub be polled at the data rate of
                        the accelerometer and gyro, if false, it is triggered
                        by INT2 line

 @return BmOK on success
         BmEINVAL if number of sensor_hub_items is larger than 
         MAX_SENSOR_HUB_SENSORS or fifo_threshold is larger than what is allowed
         BmEACCES if bus operations fail when attempting communication to imu
 */
BmErr LSM6DSV::start_stream(std::span<LSM6DSVSensorHub> sensor_hub_items, size_t fifo_threshold,
                            bool sensor_hub_poll) {
  // Start in high performance mode
  lsm6dsv_return_on_err(lsm6dsv_xl_mode_set(&m_ctx, LSM6DSV_XL_HIGH_PERFORMANCE_MD));
  lsm6dsv_return_on_err(lsm6dsv_gy_mode_set(&m_ctx, LSM6DSV_GY_HIGH_PERFORMANCE_MD));

  // Configure sensor hub
  size_t sensor_hub_count = sensor_hub_items.size();
  if (sensor_hub_count > MAX_SENSOR_HUB_SENSORS) {
    return BmEINVAL;
  }

  static constexpr uint16_t fifo_max_elements = 256;
  if (fifo_threshold > (fifo_max_elements / (READING_COUNT + sensor_hub_count))) {
    return BmEINVAL;
  }

  if (sensor_hub_count) {

    // Initialize all sensor hub elements first
    for (auto &item : sensor_hub_items) {
      // Set the argument used to point to the address of the device
      AbstractSensorInterface *sensor = item.sensor;
      sensor->m_arg = &sensor->m_address;
      sensor->set_bus(this);
      sensor->init();
      // Once data collection begins this device should become unreachable or
      // else data collection will need to be restarted
      sensor->set_bus(nullptr);
    }

    // Batch sensor hub registers to read
    lsm6dsv_sh_cfg_read_t sh_cfg_read;
    for (uint8_t i = 0; i < sensor_hub_count; i++) {
      LSM6DSVSensorHub *item = &sensor_hub_items[i];
      sh_cfg_read.slv_add = item->sensor->m_address;
      sh_cfg_read.slv_subadd = item->reg;
      sh_cfg_read.slv_len = item->len;
      lsm6dsv_return_on_err(lsm6dsv_sh_slv_cfg_read(&m_ctx, i, &sh_cfg_read));
      lsm6dsv_return_on_err(lsm6dsv_fifo_sh_batch_slave_set(&m_ctx, i, PROPERTY_ENABLE));
      m_sensor_hub[i].sensor = item->sensor;
    }

    // Configure sensor hub batch data rate if triggered by accelerometer or gyro
    lsm6dsv_return_on_err(lsm6dsv_sh_data_rate_set(&m_ctx, LSM6DSV_SH_60Hz));

    // This must be set to enable reading from device 0
    lsm6dsv_return_on_err(lsm6dsv_sh_write_mode_set(&m_ctx, LSM6DSV_ONLY_FIRST_CYCLE));

    lsm6dsv_return_on_err(lsm6dsv_sh_slave_connected_set(
        &m_ctx, static_cast<lsm6dsv_sh_slave_connected_t>(sensor_hub_count - 1)));
    lsm6dsv_return_on_err(lsm6dsv_sh_master_set(&m_ctx, PROPERTY_ENABLE));
  }

  // Set the accelerometer resolution, the lower (ex: 2g) the more sensitive
  lsm6dsv_return_on_err(lsm6dsv_xl_full_scale_set(&m_ctx, m_cfg.accelerometer.scale));

  // Set the gyro resolution
  lsm6dsv_return_on_err(lsm6dsv_gy_full_scale_set(&m_ctx, m_cfg.gyro.scale));

  // Set FIFO threshold
  lsm6dsv_return_on_err(lsm6dsv_fifo_batch_counter_threshold_set(&m_ctx, fifo_threshold));
  lsm6dsv_return_on_err(lsm6dsv_fifo_mode_set(&m_ctx, LSM6DSV_STREAM_MODE));

  // Set FIFO batch output data rate for accelerometer and gyro
  lsm6dsv_fifo_xl_batch_t batch_xl_dr = static_cast<lsm6dsv_fifo_xl_batch_t>(m_cfg.sample_rate);
  lsm6dsv_return_on_err(lsm6dsv_fifo_xl_batch_set(&m_ctx, batch_xl_dr));
  lsm6dsv_fifo_gy_batch_t batch_gy_dr = static_cast<lsm6dsv_fifo_gy_batch_t>(m_cfg.sample_rate);
  lsm6dsv_return_on_err(lsm6dsv_fifo_gy_batch_set(&m_ctx, batch_gy_dr));

  // Set filter settings, lp1 is only available when gyro is not in low power mode
  // Note: these are disabled by default
  lsm6dsv_filt_settling_mask_t filt_settling_mask = {};
  filt_settling_mask.drdy = PROPERTY_DISABLE;
  filt_settling_mask.irq_xl = PROPERTY_DISABLE;
  filt_settling_mask.irq_g = PROPERTY_DISABLE;
  lsm6dsv_return_on_err(lsm6dsv_filt_settling_mask_set(&m_ctx, filt_settling_mask));
  lsm6dsv_return_on_err(lsm6dsv_filt_gy_lp1_set(&m_ctx, PROPERTY_DISABLE));
  lsm6dsv_return_on_err(lsm6dsv_filt_gy_lp1_bandwidth_set(&m_ctx, LSM6DSV_GY_MEDIUM));
  lsm6dsv_return_on_err(lsm6dsv_filt_xl_lp2_set(&m_ctx, PROPERTY_DISABLE));
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
  lsm6dsv_return_on_err(lsm6dsv_xl_mode_set(&m_ctx, m_cfg.accelerometer.mode));
  lsm6dsv_return_on_err(lsm6dsv_gy_mode_set(&m_ctx, m_cfg.gyro.mode));

  // Configure sensor hub to trigger from interrupt INT2, by default this is active low
  lsm6dsv_sh_syncro_mode_t trigger = LSM6DSV_SH_TRIG_INT2;
  if (sensor_hub_poll) {
    trigger = LSM6DSV_SH_TRG_XL_GY_DRDY;
  } else {
    lsm6dsv_return_on_err(lsm6dsv_den_polarity_set(&m_ctx, LSM6DSV_DEN_ACT_LOW));
  }
  lsm6dsv_return_on_err(lsm6dsv_sh_syncro_mode_set(&m_ctx, trigger));

  // Set data rate of accelerometer and gyro
  lsm6dsv_return_on_err(lsm6dsv_xl_data_rate_set(&m_ctx, m_cfg.sample_rate));
  lsm6dsv_return_on_err(lsm6dsv_gy_data_rate_set(&m_ctx, m_cfg.sample_rate));

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

/*!
 @brief Handle collecting data from the FIFO

 @details Waits for an interrupt to occur indicating data is ready, once
          available the FIFO will be drained and the readings will be queued.
          Each reading will have:
            - A gyroscope reading
            - An accelerometer reading
            - A timestamp reading in nanoseconds since the LSM6DSV has booted
            - The last temperature reading
          This should be run in a task dedicated to this driver as it blocks
          with a semaphore. Readings can be taken asyn with get_reading

 @see get_reading

 @return BmOK on success
         BmEACCES if bus operations fail when attempting communication to imu
 */
BmErr LSM6DSV::stream_handle(void) {
  uint16_t num = 0;
  lsm6dsv_fifo_status_t fifo_status;

  if (!m_streaming_sem || !m_queue_mut || !m_reading_sem) {
    return BmENODEV;
  }

  bm_semaphore_take(m_streaming_sem, BM_MAX_DELAY_UINT32);

  lsm6dsv_return_on_err(lsm6dsv_fifo_status_get(&m_ctx, &fifo_status));

  num = fifo_status.fifo_level;
  LSM6DSVReading reading = {};
  uint8_t samples_set = 0;

  uint32_t i = 0;
  lsm6dsv_read_reg(&m_ctx, LSM6DSV_FIFO_DATA_OUT_TAG, m_fifo_buf, 7 * num);

  while (num--) {
    uint8_t *data = &m_fifo_buf[i];
    lsm6dsv_fifo_data_out_tag_t tag_data;
    memcpy(&tag_data, &data[0], sizeof(lsm6dsv_fifo_data_out_tag_t));
    uint8_t tag = tag_data.tag_sensor;
    data++;
    i += 7;

    int16_t datax = static_cast<int16_t>(le_uint8_to_uint16(&data[0]));
    int16_t datay = static_cast<int16_t>(le_uint8_to_uint16(&data[2]));
    int16_t dataz = static_cast<int16_t>(le_uint8_to_uint16(&data[4]));
    uint32_t ts;
    uint8_t sensor_nack_idx;
    ConverterCb cb;

    switch (tag) {
    case LSM6DSV_GY_NC_TAG:
      cb = gyro_convert(m_cfg.gyro.scale);
      reading.gyro.x = cb(datax);
      reading.gyro.y = cb(datay);
      reading.gyro.z = cb(dataz);
      samples_set++;
      break;
    case LSM6DSV_XL_NC_TAG:
      cb = accelerometer_convert(m_cfg.accelerometer.scale);
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
      ts = le_uint8_to_uint32(&data[0]);

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
    if (samples_set >= READING_COUNT) {
      // Use latest temperature value
      reading.temp = m_temperature_dc;

      bm_semaphore_take(m_queue_mut, BM_MAX_DELAY_UINT32);
      q_enqueue(&m_reading_queue, &reading, sizeof(LSM6DSVReading));
      bm_semaphore_give(m_queue_mut);
      samples_set = 0;
      reading = {};
    }

    // Set sensor hub data, ref: 9.6.1 of AN5922
    if (tag >= LSM6DSV_SENSORHUB_SLAVE0_TAG && tag <= LSM6DSV_SENSORHUB_SLAVE3_TAG) {
      uint8_t sensor_set_idx = tag - LSM6DSV_SENSORHUB_SLAVE0_TAG;
      AbstractSensorInterface *sensor = m_sensor_hub[sensor_set_idx].sensor;
      if (sensor) {
        sensor->set_data(data, sizeof(data));
      }
    }
  }

  bm_semaphore_give(m_reading_sem);

  return BmOK;
}

/*!
 @brief Determine if data is ready to be read from FIFO

 @param timeout_ms

 @return BmOK if data is available in FIFO to read
         BmETIMEDOUT if data is not ready
         BmENODEV if LSM6DSV was not initialized
 */
BmErr LSM6DSV::reading_ready(uint32_t timeout_ms) {
  if (!m_reading_sem) {
    return BmENODEV;
  }

  return bm_semaphore_take(m_reading_sem, timeout_ms);
}

/*!
 @brief Obtain a reading collected in stream_handle

 @details Grabs a single reading in the reading queue.

 @param reading oldest reading in the queue if successful

 @return BmOK on success
         BmEINVAL if invalid parameters
         BmENODATA if there are no elements in the queue to dequeue
         BmENOMEM if size of data is smaller than the size of the element being
                  dequeued
         BmENODEV if LSM6DSV was not initialized
 */
BmErr LSM6DSV::get_reading(LSM6DSVReading *reading) {
  if (!reading) {
    return BmEINVAL;
  }

  if (!m_queue_mut) {
    return BmENODEV;
  }

  bm_semaphore_take(m_queue_mut, BM_MAX_DELAY_UINT32);
  BmErr err = q_dequeue(&m_reading_queue, reading, sizeof(LSM6DSVReading));
  bm_semaphore_give(m_queue_mut);

  return err;
}

void LSM6DSV::begin() { begin_sensor_hub(); }

BmErr LSM6DSV::read(uint8_t reg, uint8_t *buf, size_t len, void *arg) {
  return read_sensor_hub(reg, buf, len, arg);
}
BmErr LSM6DSV::write(uint8_t reg, const uint8_t *buf, size_t len, void *arg) {
  return write_sensor_hub(reg, buf, len, arg);
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

BmErr LSM6DSV::write_sensor_hub(uint8_t reg, const uint8_t *data, size_t len, void *arg) {
  // Always 1 byte write
  (void)len;

  // Set the tx data rate high to reduce latency in this function
  lsm6dsv_return_on_err(lsm6dsv_sh_data_rate_set(&m_ctx, LSM6DSV_SH_120Hz));

  lsm6dsv_sh_cfg_write_t sh_cfg_write;
  sh_cfg_write.slv0_add = *static_cast<uint8_t *>(arg);
  sh_cfg_write.slv0_subadd = static_cast<uint8_t>(reg);
  sh_cfg_write.slv0_data = *data;
  lsm6dsv_return_on_err(lsm6dsv_sh_cfg_write(&m_ctx, &sh_cfg_write));

  // Enable I2C master mode
  lsm6dsv_return_on_err(lsm6dsv_sh_master_set(&m_ctx, PROPERTY_ENABLE));

  // Wait until write is performed
  return poll_bit(lsm6dsv_sh_status_get, lsm6dsv_status_master_t, wr_once_done);
}

BmErr LSM6DSV::read_sensor_hub(uint8_t reg, uint8_t *data, size_t len, void *arg) {
  int16_t raw_xl[3];
  int32_t ret;

  lsm6dsv_sh_cfg_read_t sh_cfg_read;
  sh_cfg_read.slv_add = *static_cast<uint8_t *>(arg);
  sh_cfg_read.slv_subadd = static_cast<uint8_t>(reg);
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
  // Disable I2C master
  lsm6dsv_sh_master_set(&m_ctx, PROPERTY_DISABLE);

  bm_semaphore_give(m_sensor_hub_mut);
}
