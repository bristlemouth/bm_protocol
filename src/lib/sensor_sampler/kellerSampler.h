#ifndef __KELLER_SAMPLER_H__
#define __KELLER_SAMPLER_H__

#include "io.h"
#include "protected_i2c.h"
#include "xld.h"

typedef void (*KellerSamplerCb)(float mbar, float temp);
typedef struct {
  uint32_t sample_rate;
} KellerSamplerCfg;

class KellerSampler : public SensorInterfaceBus {
public:
  KellerSampler(I2CInterface_t *i2c, IOPinHandle_t *int_pin, KellerSamplerCb cb);

  void set_cfg(KellerSamplerCfg cfg);

  XLD m_xld;
  struct {
    I2CInterface_t *i2c;
    IOPinHandle_t *isr;
    KellerSamplerCb cb;
  } m_ctx;

  KellerSamplerCfg m_cfg = {
      .sample_rate = 10,
  };

private:
  // Max sample rate in hz
  static constexpr uint8_t MAX_SAMPLE_RATE = 125;

  void begin(void) override;
  BmErr read(uint8_t address, uint8_t *buf, size_t len, void *arg) override;
  BmErr write(uint8_t address, const uint8_t *buf, size_t len, void *arg) override;
  void end(void) override;
};

BmErr keller_sampler_add(KellerSampler *sampler);

#endif
