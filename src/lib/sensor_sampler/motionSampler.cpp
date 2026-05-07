#include "motionSampler.h"
#include "lpm.h"
#include <array>
#include <string.h>

#ifndef MOTION_TASK_PRIORITY
#define MOTION_TASK_PRIORITY 3
#endif

static struct {
  SPIInterface_t *spi;
  IOPinHandle_t *cs;
  IOPinHandle_t *isr;
  uint32_t lpm_mask;
} ctx = {};

class SpiBus : public SensorInterfaceBus {
  void begin(void) override { IOWrite(ctx.cs, 0); }

  BmErr read(uint8_t reg, uint8_t *buf, size_t len, void *arg) override {
    (void)arg;
    reg = 1 << 7 | reg;
    spiTx(ctx.spi, NULL, 1, &reg, 100);
    return static_cast<BmErr>(spiRxNonblocking(ctx.spi, NULL, len, buf, 100));
  }

  BmErr write(uint8_t reg, const uint8_t *buf, size_t len, void *arg) override {
    (void)arg;

    spiTx(ctx.spi, NULL, 1, &reg, 100);
    return static_cast<BmErr>(spiTx(ctx.spi, NULL, len, (uint8_t *)buf, 100));
  }

  void end(void) override { IOWrite(ctx.cs, 1); }
};

static SpiBus spi_bus;
static LSM6DSV lsm6dsv(&spi_bus);

static bool lsm6dsv_isr_handle(const void *pin, uint8_t value, void *args) {
  (void)pin;
  (void)args;

  if (value) {
    lsm6dsv.handle_interrupt();
  }

  return true;
}

static void motion_task(void *arg) {
  (void)arg;
  LSM6DSV::Cfg cfg = {
      .accelerometer =
          {
              .scale = LSM6DSV_2g,
              .mode = LSM6DSV_XL_HIGH_PERFORMANCE_MD,
              .rate = LSM6DSV_ODR_AT_1920Hz,
          },
      .gyro =
          {
              .scale = LSM6DSV_125dps,
              .mode = LSM6DSV_GY_HIGH_PERFORMANCE_MD,
              .rate = LSM6DSV_ODR_AT_1920Hz,
          },
  };
  lsm6dsv.init(&cfg);

  static constexpr uint8_t num_fifo_readings = 12;

  //TODO: add LISM compass to sensor hub
  std::array<LSM6DSV::LSM6DSVSensorHub, 0> sensor_hub = {};

  lsm6dsv.start_stream(sensor_hub, num_fifo_readings);

  while (1) {
    lsm6dsv.stream_handle();
  }
}

BmErr motionSensorAdd(SPIInterface_t *spi, IOPinHandle_t *cs_pin, IOPinHandle_t *int_pin) {
  if (!spi || !cs_pin || !int_pin) {
    return BmEINVAL;
  }

  ctx.spi = spi;
  ctx.cs = cs_pin;
  ctx.isr = int_pin;

  IORegisterCallback(ctx.isr, lsm6dsv_isr_handle, nullptr);

  // Start with chip select pin high
  IOWrite(ctx.cs, 1);

  static constexpr uint32_t stack_size = 1024;
  return bm_task_create(motion_task, "motion", stack_size, NULL, MOTION_TASK_PRIORITY, NULL);
}

BmErr motionSensorDataReady(uint32_t timeout_ms) { return lsm6dsv.reading_ready(timeout_ms); }

BmErr motionSensorGet(MotionSensorReading *reading) { return lsm6dsv.get_reading(reading); }
