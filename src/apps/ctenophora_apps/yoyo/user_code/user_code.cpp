#include "user_code.h"
#include "bm_config.h"
#include "motor_process.h"
#include "pubsub.h"
#include "pwm_debug.h"
#include "uptime.h"
#include <string.h>

#define POWER_SAMPLER_PERIOD_MS (100)

#define MOTOR_CONTROL_TOPIC "motor"

void motor_sub(uint64_t node_id, const char *topic, uint16_t topic_len, const uint8_t *data,
               uint16_t data_len, uint8_t type, uint8_t version) {
  (void)node_id;
  (void)type;
  (void)version;
  const char *command = (const char *)data;

  bm_debug("Got data on topic: %.*s\n", topic_len, topic);

  if (command) {
    if (strncmp("forward", command, data_len) == 0) {
      bm_debug("Setting motor forward\n");
      set_motor_state(MOTOR_FORWARD);
    } else if (strncmp("backward", command, data_len) == 0) {
      set_motor_state(MOTOR_BACKWARD);
      bm_debug("Setting motor backward\n");
    } else if (strncmp("off", command, data_len) == 0) {
      set_motor_state(MOTOR_OFF);
      bm_debug("Turning motor off\n");
    }
  }
}

//static void back_forth(void) {
//  static uint32_t loop_time_ms = uptimeGetMs();
//  static bool forward = true;
//  static bool rest = false;
//
//  if (uptimeGetMs() - loop_time_ms >= 10000) {
//    if (rest) {
//      set_motor_state(MOTOR_OFF);
//      rest = false;
//    } else if (forward) {
//      set_motor_state(MOTOR_FORWARD);
//      forward = false;
//      rest = true;
//    } else {
//      set_motor_state(MOTOR_BACKWARD);
//      forward = true;
//      rest = true;
//    }
//    loop_time_ms = uptimeGetMs();
//  }
//}

#include "bmp5.h"
#include "bsp.h"
#include "protected_i2c.h"

BMP5_INTF_RET_TYPE bmp581_write(uint8_t reg, const uint8_t *data, uint32_t len,
                                void *intf_ptr) {
  uint8_t size = sizeof(reg) + len;
  uint8_t buf[size];

  buf[0] = reg;
  memcpy(&buf[1], data, len);

  I2CResponse_t resp = i2cTx(&i2c1, *(uint8_t *)intf_ptr, (uint8_t *)buf, size, 100);

  return resp == I2C_OK ? 0 : 1;
}

BMP5_INTF_RET_TYPE bmp581_read(uint8_t reg, uint8_t *data, uint32_t len, void *intf_ptr) {
  I2CResponse_t resp = i2cTx(&i2c1, *(uint8_t *)intf_ptr, &reg, sizeof(reg), 100);

  if (resp == I2C_OK) {
    resp = i2cRx(&i2c1, *(uint8_t *)intf_ptr, data, len, 100);
  }

  return resp == I2C_OK ? 0 : 1;
}

#include "bm_os.h"
void bmp581_delay_us(uint32_t us, void *intf_ptr) {
  (void)intf_ptr;
  delay_us(us);
}

static struct bmp5_dev dev = {};

void bmp_sensor_init(void) {
  static uint8_t addr = I2C_BMP581_ADDR;
  dev.read = bmp581_read;
  dev.write = bmp581_write;
  dev.delay_us = bmp581_delay_us;
  dev.intf_ptr = &addr;
  dev.intf = BMP5_I2C_INTF;
  bmp5_soft_reset(&dev);
  bmp5_init(&dev);

  bmp5_set_power_mode(BMP5_POWERMODE_STANDBY, &dev);

  const struct bmp5_osr_odr_press_config osr_odf_press_cfg = {
      .osr_t = BMP5_OVERSAMPLING_2X,
      .osr_p = BMP5_OVERSAMPLING_2X,
      .press_en = BMP5_ENABLE,
      .odr = BMP5_ODR_240_HZ,
  };
  bmp5_set_osr_odr_press_config(&osr_odf_press_cfg, &dev);

  struct bmp5_osr_odr_eff osr_odr_eff;
  bmp5_get_osr_odr_eff(&osr_odr_eff, &dev);
  bm_debug("Is valid: %u\n", osr_odr_eff.odr_is_valid);

  const struct bmp5_iir_config iir_cfg = {
      .set_iir_t = BMP5_IIR_FILTER_COEFF_7,
      .set_iir_p = BMP5_IIR_FILTER_COEFF_7,
      .shdw_set_iir_t = BMP5_DISABLE,
      .shdw_set_iir_p = BMP5_DISABLE,
      .iir_flush_forced_en = BMP5_DISABLE,
  };
  bmp5_set_iir_config(&iir_cfg, &dev);

  bmp5_configure_interrupt(BMP5_PULSED, BMP5_ACTIVE_LOW, BMP5_INTR_PUSH_PULL, BMP5_INTR_ENABLE,
                           &dev);

  const struct bmp5_int_source_select int_source_select {
    .drdy_en = BMP5_ENABLE, .fifo_full_en = BMP5_DISABLE, .fifo_thres_en = BMP5_DISABLE,
    .oor_press_en = BMP5_DISABLE,
  };
  bmp5_int_source_select(&int_source_select, &dev);
  bmp5_int_source_select(&int_source_select, &dev);

  bmp5_set_power_mode(BMP5_POWERMODE_NORMAL, &dev);
}

void get_bmp_sensor_data(void) {
  uint8_t int_status;
  bmp5_get_interrupt_status(&int_status, &dev);

  if (int_status & BMP5_INT_ASSERTED_DRDY) {
    struct bmp5_osr_odr_press_config osr_odf_press_cfg = {};

    struct bmp5_sensor_data sensor_data = {};
    bmp5_get_osr_odr_press_config(&osr_odf_press_cfg, &dev);
    bmp5_get_sensor_data(&sensor_data, &osr_odf_press_cfg, &dev);

    bm_debug("Sensor Temp: %f, Pressure: %f\n", sensor_data.temperature, sensor_data.pressure);
  }
}

extern "C" {
#include "sh2.h"
#include "sh2_SensorValue.h"
#include "sh2_err.h"
#include "sh2_util.h"
}
#include "util.h"
#define BNO085_ADDRESS (0x4B)
static BmSemaphore imu_semaphore;
static uint32_t imu_read_us;

static int bno085_open(sh2_Hal_t *self) {
  (void)self;
  return 0;
}
static void bno085_close(sh2_Hal_t *self) { (void)self; }
static int bno085_write(sh2_Hal_t *self, uint8_t *buf, unsigned len) {
  (void)self;

  i2cTx(&i2c1, BNO085_ADDRESS, buf, len, 100);

  return len;
}
static int bno085_read(sh2_Hal_t *self, uint8_t *buf, unsigned len, uint32_t *t_us) {
  (void)self;
  (void)t_us;
  // Read the cargo length header first (4 bytes)
  _Alignas(4) static uint8_t header[4];
  if (i2cRx(&i2c1, BNO085_ADDRESS, header, 4, 1000) != I2C_OK) {
    return 0;
  }

  // Extract the cargo length from the header
  uint16_t cargo_len = (header[1] << 8) | header[0];
  cargo_len &= 0x7FFF; // Clear continuation bit

  // Check if there's actual data
  if (cargo_len == 0) {
    return 0;
  }

  // Read the cargo data
  if (cargo_len > len) {
    cargo_len = len;
  }

  _Alignas(4) static uint8_t cpy_buf[1024];
  if (i2cRx(&i2c1, BNO085_ADDRESS, cpy_buf, cargo_len, 1000) != I2C_OK) {
    return 0;
  }
  memcpy(buf, cpy_buf, cargo_len);

  return cargo_len; // Return number of bytes read
}

static uint32_t bno085_get_time_us(sh2_Hal_t *self) {
  (void)self;

  return (uint32_t)bm_ticks_to_ms(bm_get_tick_count()) * 1000;
}

static bool imu_int_cb(const void *pinHandle, uint8_t value, void *args) {
  (void)pinHandle;
  (void)value;
  (void)args;
  BaseType_t higher_priority_task_woken = false;
  imu_read_us = (uint32_t)bm_ticks_to_ms(bm_get_tick_count()) * 1000;
  xSemaphoreGiveFromISR(imu_semaphore, &higher_priority_task_woken);
  portYIELD_FROM_ISR(higher_priority_task_woken);

  return true;
}

static void bno085_event(void *cookie, sh2_AsyncEvent_t *event) {
  (void)cookie;
  (void)event;
  //printf("BNO085 event cb info:\n");
  //printf("id: %" PRIu32 "\n", event->eventId);
  //printf("event: %u\n", event->shtpEvent);
}

static void printEvent(const sh2_SensorEvent_t *event) {
  int rc;
  sh2_SensorValue_t value;
  float scaleRadToDeg = 180.0 / 3.14159265358;
  float r, i, j, k, acc_deg, x, y, z;
  float t;
  static int skip = 0;

  rc = sh2_decodeSensorEvent(&value, event);
  if (rc != SH2_OK) {
    printf("Error decoding sensor event: %d\n", rc);
    return;
  }

  t = value.timestamp / 1000000.0; // time in seconds.
  switch (value.sensorId) {
  case SH2_RAW_ACCELEROMETER:
    printf("%8.4f Raw acc: %d %d %d time_us:%lu\n", (double)t, value.un.rawAccelerometer.x,
           value.un.rawAccelerometer.y, value.un.rawAccelerometer.z,
           value.un.rawAccelerometer.timestamp);
    break;

  case SH2_ACCELEROMETER:
    printf("%8.4f Acc: %f %f %f\n", (double)t, (double)value.un.accelerometer.x,
           (double)value.un.accelerometer.y, (double)value.un.accelerometer.z);
    break;

  case SH2_RAW_GYROSCOPE:
    printf("%8.4f Raw gyro: x:%d y:%d z:%d temp:%d time_us:%lu\n", (double)t,
           value.un.rawGyroscope.x, value.un.rawGyroscope.y, value.un.rawGyroscope.z,
           value.un.rawGyroscope.temperature, value.un.rawGyroscope.timestamp);
    break;

  case SH2_ROTATION_VECTOR:
    r = value.un.rotationVector.real;
    i = value.un.rotationVector.i;
    j = value.un.rotationVector.j;
    k = value.un.rotationVector.k;
    acc_deg = scaleRadToDeg * value.un.rotationVector.accuracy;
    printf("%8.4f Rotation Vector: "
           "r:%0.6f i:%0.6f j:%0.6f k:%0.6f (acc: %0.6f deg)\n",
           (double)t, (double)r, (double)i, (double)j, (double)k, (double)acc_deg);
    break;
  case SH2_GAME_ROTATION_VECTOR:
    r = value.un.gameRotationVector.real;
    i = value.un.gameRotationVector.i;
    j = value.un.gameRotationVector.j;
    k = value.un.gameRotationVector.k;
    printf("%8.4f GRV: "
           "r:%0.6f i:%0.6f j:%0.6f k:%0.6f\n",
           (double)t, (double)r, (double)i, (double)j, (double)k);
    break;
  case SH2_GYROSCOPE_CALIBRATED:
    x = value.un.gyroscope.x;
    y = value.un.gyroscope.y;
    z = value.un.gyroscope.z;
    printf("%8.4f GYRO: "
           "x:%0.6f y:%0.6f z:%0.6f\n",
           (double)t, (double)x, (double)y, (double)z);
    break;
  case SH2_GYROSCOPE_UNCALIBRATED:
    x = value.un.gyroscopeUncal.x;
    y = value.un.gyroscopeUncal.y;
    z = value.un.gyroscopeUncal.z;
    printf("%8.4f GYRO_UNCAL: "
           "x:%0.6f y:%0.6f z:%0.6f\n",
           (double)t, (double)x, (double)y, (double)z);
    break;
  case SH2_GYRO_INTEGRATED_RV:
    // These come at 1kHz, too fast to print all of them.
    // So only print every 10th one
    skip++;
    if (skip == 10) {
      skip = 0;
      r = value.un.gyroIntegratedRV.real;
      i = value.un.gyroIntegratedRV.i;
      j = value.un.gyroIntegratedRV.j;
      k = value.un.gyroIntegratedRV.k;
      x = value.un.gyroIntegratedRV.angVelX;
      y = value.un.gyroIntegratedRV.angVelY;
      z = value.un.gyroIntegratedRV.angVelZ;
      printf("%8.4f Gyro Integrated RV: "
             "r:%0.6f i:%0.6f j:%0.6f k:%0.6f x:%0.6f y:%0.6f z:%0.6f\n",
             (double)t, (double)r, (double)i, (double)j, (double)k, (double)x, (double)y,
             (double)z);
    }
    break;
  case SH2_IZRO_MOTION_REQUEST:
    printf("IZRO Request: intent:%d, request:%d\n", value.un.izroRequest.intent,
           value.un.izroRequest.request);
    break;
  case SH2_SHAKE_DETECTOR:
    printf("Shake Axis: %c%c%c\n", (value.un.shakeDetector.shake & SHAKE_X) ? 'X' : '.',
           (value.un.shakeDetector.shake & SHAKE_Y) ? 'Y' : '.',
           (value.un.shakeDetector.shake & SHAKE_Z) ? 'Z' : '.');

    break;
  case SH2_STABILITY_CLASSIFIER:
    printf("Stability Classification: %d\n", value.un.stabilityClassifier.classification);
    break;
  case SH2_STABILITY_DETECTOR:
    printf("Stability Detector: %d\n", value.un.stabilityDetector.stability);
    break;
  default:
    printf("Unknown sensor: %d\n", value.sensorId);
    break;
  }
}

static void startReports() {
  int status;

  // Each entry of sensorConfig[] represents one sensor to be configured in the loop below
  static const struct {
    int sensorId;
    sh2_SensorConfig_t config;
  } sensorConfig[] = {
      // Game Rotation Vector, 100Hz
      {SH2_GAME_ROTATION_VECTOR,
       {
           .changeSensitivityEnabled = false,
           .changeSensitivityRelative = false,
           .wakeupEnabled = false,
           .alwaysOnEnabled = false,
           .sniffEnabled = false,
           .changeSensitivity = false,
           .reportInterval_us = 10000,
           .batchInterval_us = 0,
           .sensorSpecific = 0,
       }},

      // Stability Detector, 100 Hz, changeSensitivityEnabled
      // {SH2_STABILITY_DETECTOR, {.reportInterval_us = 10000, .changeSensitivityEnabled = true}},

      // Raw accel, 100 Hz
      // {SH2_RAW_ACCELEROMETER, {.reportInterval_us = 10000}},

      // Raw gyroscope, 100 Hz
      {SH2_RAW_GYROSCOPE,
       {
           .changeSensitivityEnabled = false,
           .changeSensitivityRelative = false,
           .wakeupEnabled = false,
           .alwaysOnEnabled = false,
           .sniffEnabled = false,
           .changeSensitivity = false,
           .reportInterval_us = 10000,
           .batchInterval_us = 0,
           .sensorSpecific = 0,
       }},

      // Rotation Vector, 100 Hz
      // {SH2_ROTATION_VECTOR, {.reportInterval_us = 10000}},

      // Gyro Integrated Rotation Vector, 100 Hz
      // {SH2_GYRO_INTEGRATED_RV, {.reportInterval_us = 10000}},

      // Motion requests for Interactive Zero Reference Offset cal
      // {SH2_IZRO_MOTION_REQUEST, {.reportInterval_us = 10000}},

      // Shake detector
      // {SH2_SHAKE_DETECTOR, {.reportInterval_us = 10000}},
  };

  for (uint32_t n = 0; n < array_size(sensorConfig); n++) {
    int sensorId = sensorConfig[n].sensorId;

    status = sh2_setSensorConfig(sensorId, &sensorConfig[n].config);
    if (status != 0) {
      printf("Error while enabling sensor %d\n", sensorId);
    }
  }
}

static void sensorHandler(void *cookie, sh2_SensorEvent_t *pEvent) {
  (void)cookie;
  printEvent(pEvent);
}

static void reportProdIds(void) {
  sh2_ProductIds_t prodIds;
  int status;

  status = sh2_getProdIds(&prodIds);

  if (status < 0) {
    printf("Error from sh2_getProdIds.\n");
    return;
  }

  // Report the results
  for (int n = 0; n < prodIds.numEntries; n++) {
    printf("Part %lu : Version %d.%d.%d Build %lu\n", prodIds.entry[n].swPartNumber,
           prodIds.entry[n].swVersionMajor, prodIds.entry[n].swVersionMinor,
           prodIds.entry[n].swVersionPatch, prodIds.entry[n].swBuildNumber);
  }
}

void imu_task(void *arg) {
  (void)arg;
  sh2_Hal_t hal = {
      bno085_open, bno085_close, bno085_read, bno085_write, bno085_get_time_us,
  };

  sh2_open(&hal, bno085_event, NULL);
  sh2_setSensorCallback(sensorHandler, NULL);
  reportProdIds();
  startReports();

  while (1) {
    if (bm_semaphore_take(imu_semaphore, 1000) != BmOK) {
      printf("Timeout waiting on semaphore\n");
      continue;
    }
    sh2_service();
  }
}

void setup(void) {
  lpmPeripheralActive(LPM_BOOT);

  pwm_debug_init();
  //motor_init();

  bm_sub(MOTOR_CONTROL_TOPIC, motor_sub);

  // Enable power to the board
  IOWrite(&IMU_BOOT, 1);
  IOWrite(&VBUS_EN, 0);

  bmp_sensor_init();

  IORegisterCallback(&IMU_INT, imu_int_cb, NULL);

  imu_semaphore = bm_semaphore_create();
  bm_task_create(imu_task, "IMU", 4096, NULL, configMAX_PRIORITIES - 1, NULL);
}

void loop(void) {}
