#include "sensors.h"
#include "bm_config.h"
#include "bsp.h"
#include "configuration.h"
#include "kellerSampler.h"
#include "lpm.h"
//#include "motionSampler.h"
#include <stdbool.h>
#include <stdint.h>
#include "bno085.h"
#include "ina232.h"

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

//static INA::INA232 debugIna1(&i2c1, I2C_INA_PODL_ADDR);
//static INA::INA232 *debugIna[NUM_INA232_DEV] = {
//    &debugIna1,
//};
//static MotionSampler motion(&spi1, &BM_CS, &BM_INT);
static Bno085 imu(&spi1, &BM_CS, &BM_INT, &I2C_MUX_RESET, &GPIO1, &GPIO2);

static void imuEventCallback(void *cookie, sh2_AsyncEvent_t *e) {
  (void)cookie;
  switch (e->eventId) {
    case SH2_RESET:
      printf("BNO evt: RESET\n");
      break;
    case SH2_SHTP_EVENT:
      printf("BNO evt: SHTP error %d\n", e->shtpEvent);
      break;
    case SH2_GET_FEATURE_RESP:
      printf("BNO evt: FEATURE_RESP id=%d interval=%lu\n",
             e->sh2SensorConfigResp.sensorId,
             (unsigned long)e->sh2SensorConfigResp.sensorConfig.reportInterval_us);
      break;
    default:
      printf("BNO evt: id=%lu\n", (unsigned long)e->eventId);
      break;
  }
}

static void imuSensorCallback(void *cookie, sh2_SensorEvent_t *event) {
  (void)cookie;
  sh2_SensorValue_t v;
  if (sh2_decodeSensorEvent(&v, event) != SH2_OK) {
    printf("BNO085: decode failed\n");
    return;
  }
  switch (v.sensorId) {
    case SH2_ACCELEROMETER:  // milli-m/s^2
      printf("accel: x=%ld y=%ld z=%ld (mm/s^2) acc=%u\n",
              (long)(v.un.accelerometer.x * 1000.0f),
              (long)(v.un.accelerometer.y * 1000.0f),
              (long)(v.un.accelerometer.z * 1000.0f),
              (unsigned)(v.status & 0x03));
      break;
    case SH2_GYROSCOPE_CALIBRATED:  // milli-rad/s
      printf("gyro:  x=%ld y=%ld z=%ld (mrad/s) acc=%u\n",
              (long)(v.un.gyroscope.x * 1000.0f),
              (long)(v.un.gyroscope.y * 1000.0f),
              (long)(v.un.gyroscope.z * 1000.0f),
              (unsigned)(v.status & 0x03));
      break;
    case SH2_MAGNETIC_FIELD_CALIBRATED:  // milli-microTesla
      printf("mag:   x=%ld y=%ld z=%ld (uT/1000) acc=%u\n",
              (long)(v.un.magneticField.x * 1000.0f),
              (long)(v.un.magneticField.y * 1000.0f),
              (long)(v.un.magneticField.z * 1000.0f),
              (unsigned)(v.status & 0x03));
      break;
    default:
      printf("BNO085: report 0x%02x\n", v.sensorId);
      break;
  }
}

//TODO: change to GPIO2 when new boards come in
static KellerSampler keller(&i2c1, &IOEXP_INT, keller_sample_cb);

void sensorsInit(void) {
  // Wait for the 3V3 rail to stabilize before communicating with the mux
  vTaskDelay(pdMS_TO_TICKS(5));

  // Power monitor
  //powerSamplerInit(debugIna);

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
  motion.set_cfg(cfg);
  motion_sampler_add(&motion);

  init_gpio2();
  keller_sampler_add(&keller);
}

void sensorsHandle(void) {
  if (motion.data_ready()) {
    IMUReading imu = {};
    CompassReading compass = {};
    while (motion.data_get(&imu) == BmOK) {
      //bm_debug("imu: %" PRIu64 ",%f,%f,%f,%f,%f,%f\n", imu.ns, imu.acc.x, imu.acc.y, imu.acc.z,
      //         imu.gyro.x, imu.gyro.y, imu.gyro.z);
    }
    while (motion.data_get(&compass) == BmOK) {
      //bm_debug("compass: %" PRIu64 ",%f,%f,%f\n", compass.ns, compass.x, compass.y, compass.z);
    }
  }
}
