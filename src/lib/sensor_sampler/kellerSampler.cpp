#include "kellerSampler.h"
#include "task.h"
#include "uptime.h"

#ifndef KELLER_TASK_PRIORITY
#define KELLER_TASK_PRIORITY 2
#endif

KellerSampler::KellerSampler(I2CInterface_t *i2c, IOPinHandle_t *int_pin, KellerSamplerCb cb)
    : m_xld(this, NULL) {
  m_ctx.i2c = i2c;
  m_ctx.isr = int_pin;
  m_ctx.cb = cb;
}

/*!
 @brief Set the configuration used for the Keller sensor

 @details Must be invoked before keller_sampler_add

 @param cfg Configuration to set
 */
void KellerSampler::set_cfg(KellerSamplerCfg cfg) { m_cfg = cfg; }

/*!
 @brief Begin sensor communication handler
 */
void KellerSampler::begin(void) {}

/*!
 @brief Read from the sensor using I2C

 @details The LSM6DSV requires the most significant bit to be set to 1 in order
          to indicate a read from the device.

 @param address Device address to read
 @param buf Buffer size
 @param len Length of buffer
 @param arg unused

 @return BmOK on success
         BmErr on failure
 */
BmErr KellerSampler::read(uint8_t address, uint8_t *buf, size_t len, void *arg) {
  (void)arg;

  return static_cast<BmErr>(i2cRx(m_ctx.i2c, address, buf, len, 100));
}

/*!
 @brief Write to the sensor using I2C

 @param address Device address to write
 @param buf Buffer size
 @param len Length of buffer
 @param arg unused

 @return BmOK on success
         BmErr on failure
 */
BmErr KellerSampler::write(uint8_t address, const uint8_t *buf, size_t len, void *arg) {
  (void)arg;

  return static_cast<BmErr>(i2cTx(m_ctx.i2c, address, const_cast<uint8_t *>(buf), len, 100));
}

/*!
 @brief End sensor communication handler
 */
void KellerSampler::end(void) {}

static bool xld_isr_handle(const void *pin, uint8_t value, void *args) {
  (void)pin;
  KellerSampler *sampler = static_cast<KellerSampler *>(args);

  // Active high interrupt
  if (value) {
    sampler->m_xld.handle_interrupt();
  }

  return true;
}

static void keller_task(void *arg) {
  KellerSampler *sampler = static_cast<KellerSampler *>(arg);
  XLD *xld = &sampler->m_xld;
  KellerSamplerCb cb = sampler->m_ctx.cb;
  uint32_t delay_ms = 1000 / sampler->m_cfg.sample_rate;

  if (xld->init() != BmOK) {
    vTaskDelete(NULL);
    return;
  }

  float mbar;
  float temp;
  while (1) {
    uint32_t uptime_begin = uptimeGetMs();
    xld->request_reading();
    if (xld->get_reading(&mbar, &temp) == BmOK && cb) {
      cb(mbar, temp);
    }
    uint32_t uptime_end = uptimeGetMs();

    // Delay taking in account the time needed for conversion
    bm_delay(delay_ms - (uptime_end - uptime_begin));
  }
}

/*!
 @brief Add a keller sensor sampler

 @details Configures interrupt for the sampler ISR pin and creates an instance
          of a keller_task.

 @param sampler Sampler instance to begin sampling

 @return BmOK on success
         BmErr on failure
 */
BmErr keller_sampler_add(KellerSampler *sampler) {
  constexpr uint8_t max_sample_rate = 125;
  if (!sampler || sampler->m_cfg.sample_rate > max_sample_rate) {
    return BmEINVAL;
  }

  IORegisterCallback(sampler->m_ctx.isr, xld_isr_handle, sampler);

  static constexpr uint32_t stack_size = 512;
  return bm_task_create(keller_task, "keller", stack_size, sampler, KELLER_TASK_PRIORITY, NULL);
}
