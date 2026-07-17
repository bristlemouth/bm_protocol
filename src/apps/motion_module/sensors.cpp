#include "sensors.h"
#include "bm_config.h"
#include "bsp.h"
#include "configuration.h"
#include "kellerSampler.h"
#include "lpm.h"
#include <stdbool.h>
#include <stdint.h>
#include "ina232.h"
#if defined(IMU_BNO085)
#include "bno085Sampler.h"
#else
#include "motionSampler.h"
#endif

#include "uptime.h"

#include "main.h"
#include "stm32_io.h"
#include "stm32u5xx_ll_exti.h"
extern "C" void EXTI13_IRQHandler(void) {
  BaseType_t rval = pdFALSE;
  if (LL_EXTI_IsActiveFallingFlag_0_31(LL_EXTI_LINE_13) != RESET) {
    LL_EXTI_ClearFallingFlag_0_31(LL_EXTI_LINE_13);
    rval |= STM32IOHandleInterrupt((const STM32Pin_t *)GPIO2.pin);
  }
  if (LL_EXTI_IsActiveRisingFlag_0_31(LL_EXTI_LINE_13) != RESET) {
    LL_EXTI_ClearRisingFlag_0_31(LL_EXTI_LINE_13);
    rval |= STM32IOHandleInterrupt((const STM32Pin_t *)GPIO2.pin);
  }
  portYIELD_FROM_ISR(rval);
}

static void init_gpio2(void) {
  // Configure GPIO2 interrupt line
  LL_EXTI_InitTypeDef exti_cfg = {
      .Line_0_31 = LL_EXTI_LINE_13,
      .LineCommand = ENABLE,
      .Mode = LL_EXTI_MODE_IT,
      .Trigger = LL_EXTI_TRIGGER_RISING_FALLING,
  };
  LL_EXTI_SetEXTISource(LL_EXTI_EXTI_PORTC, LL_EXTI_EXTI_LINE13);
  LL_EXTI_Init(&exti_cfg);

  LL_GPIO_SetPinPull(GPIO2_GPIO_Port, GPIO2_Pin, LL_GPIO_PULL_NO);
  LL_GPIO_SetPinMode(GPIO2_GPIO_Port, GPIO2_Pin, LL_GPIO_MODE_INPUT);

  NVIC_SetPriority(EXTI13_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 6, 0));
  NVIC_EnableIRQ(EXTI13_IRQn);
}

static void keller_sample_cb(float mbar, float temp) {
  bm_debug("pressure: %" PRIu64 ",%f,%f\n", uptimeGetMicroSeconds(), mbar, temp);
}

// Sampler initialization functions (so we don't need individual headers)
void powerSamplerInit(
    INA::INA232 **sensors); // implemented in src/lib/sensor_sampler/powerSampler.cpp

static INA::INA232 debugIna1(&i2c1, I2C_INA_PODL_ADDR);
static INA::INA232 *debugIna[NUM_INA232_DEV] = {
   &debugIna1,
};


#if defined(IMU_BNO085)
static Bno085 imu(&spi1, &BM_CS, &BM_INT, &I2C_MUX_RESET, &GPIO1, &GPIO2);
#else
static MotionSampler imu(&spi1, &BM_CS, &BM_INT);
#endif

//TODO: change to GPIO2 when new boards come in
static KellerSampler keller(&i2c1, &IOEXP_INT, keller_sample_cb);

void sensorsInit(void) {
  // Wait for the 3V3 rail to stabilize before communicating with the mux
  vTaskDelay(pdMS_TO_TICKS(5));

  // Power monitor
  powerSamplerInit(debugIna);

  #if defined(IMU_BNO085)
    Bno085SamplerConfig cfg = bno085_sampler_get_default_config();
    imu.set_cfg(cfg);
    configASSERT(bno085_sampler_add(&imu) == BmOK);
  #else
  // Obtain configs for motion sensing module
  MotionSamplerConfig cfg = motion_sampler_get_default_config();
  uint32_t acc_scale = 0, gyro_scale = 0, sample_rate = 0;
  if (get_config_uint(BM_CFG_PARTITION_SYSTEM, "accScale", strlen("accScale"), &acc_scale)) {
    cfg.accelerometer.scale = static_cast<lsm6dsv_xl_full_scale_t>(acc_scale);
  }
  if (get_config_uint(BM_CFG_PARTITION_SYSTEM, "gyroScale", strlen("gyroScale"), &gyro_scale)) {
    cfg.gyro.scale = static_cast<lsm6dsv_gy_full_scale_t>(gyro_scale);
  }
  if (get_config_uint(BM_CFG_PARTITION_SYSTEM, "sampleRate", strlen("sampleRate"),
                      &sample_rate)) {
    cfg.sample_rate = static_cast<lsm6dsv_data_rate_t>(sample_rate);
  }
  imu.set_cfg(cfg);
  configASSERT(motion_sampler_add(&imu) == BmOK);
  #endif

  init_gpio2();
  keller_sampler_add(&keller);
}

void sensorsHandle(void) {
  if (imu.data_ready()) {
    IMUReading reading = {};
    CompassReading compass = {};
    while (imu.data_get(&reading) == BmOK) {
      bm_debug("imu: %" PRIu64 ",%f,%f,%f,%f,%f,%f\n", reading.ns, reading.acc.x, reading.acc.y, reading.acc.z,
               reading.gyro.x, reading.gyro.y, reading.gyro.z);
    }
    while (motion.data_get(&compass) == BmOK) {
      //bm_debug("compass: %" PRIu64 ",%f,%f,%f\n", compass.ns, compass.x, compass.y, compass.z);
    }
  }
}
