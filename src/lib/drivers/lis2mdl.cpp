#include "lis2mdl.h"
#include "app_util.h"
#include "bm_config.h"

#define lis2mdl_return_on_err(f)                                                               \
  ({                                                                                           \
    int32_t err = (f);                                                                         \
    if (err != 0) {                                                                            \
      bm_debug("lis2mdl err %d:%" PRId32 "\n", __LINE__, err);                                 \
      return BmEACCES;                                                                         \
    }                                                                                          \
  })

BmErr LIS2MDL::init(void) {
  BmErr err = q_create_static(&m_reading_queue, m_readings_buf, sizeof(m_readings_buf));
  if (err != BmOK) {
    return err;
  }

  m_queue_mut = bm_mutex_create();
  if (!m_queue_mut) {
    return BmENOMEM;
  }

  // Validate the LIS2MDL device ID
  uint8_t whoami = 0;
  lis2mdl_return_on_err(lis2mdl_device_id_get(&m_ctx, &whoami));
  if (whoami != LIS2MDL_ID) {
    return BmENODEV;
  }

  lis2mdl_return_on_err(lis2mdl_sw_reset(&m_ctx));
  lis2mdl_return_on_err(lis2mdl_block_data_update_set(&m_ctx, PROPERTY_ENABLE));
  lis2mdl_return_on_err(lis2mdl_offset_temp_comp_set(&m_ctx, PROPERTY_ENABLE));
  lis2mdl_return_on_err(lis2mdl_drdy_on_pin_set(&m_ctx, PROPERTY_ENABLE));
  lis2mdl_return_on_err(lis2mdl_operating_mode_set(&m_ctx, LIS2MDL_CONTINUOUS_MODE));
  lis2mdl_return_on_err(lis2mdl_power_mode_set(&m_ctx, m_cfg.mode));
  lis2mdl_return_on_err(lis2mdl_data_rate_set(&m_ctx, m_cfg.sample_rate));

  return BmOK;
}

BmErr LIS2MDL::set_data(const uint8_t *buf, size_t len) {

  if (!buf || len < EXPECTED_DATA_LENGTH) {
    return BmEINVAL;
  }

  int16_t datax = static_cast<int16_t>(le_uint8_to_uint16(&buf[0]));
  int16_t datay = static_cast<int16_t>(le_uint8_to_uint16(&buf[2]));
  int16_t dataz = static_cast<int16_t>(le_uint8_to_uint16(&buf[4]));

  //TODO: should we put timestamps here?
  //
  LIS2MDLReading reading = {
      .timestamp_ns = 0,
      .x = lis2mdl_from_lsb_to_mgauss(datax),
      .y = lis2mdl_from_lsb_to_mgauss(datay),
      .z = lis2mdl_from_lsb_to_mgauss(dataz),
  };

  bm_semaphore_take(m_queue_mut, BM_MAX_DELAY_UINT32);
  q_enqueue(&m_reading_queue, &reading, sizeof(LIS2MDLReading));
  bm_semaphore_give(m_queue_mut);

  return BmOK;
}
