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

void setup(void) {
  lpmPeripheralActive(LPM_BOOT);

  pwm_debug_init();
  //motor_init();

  bm_sub(MOTOR_CONTROL_TOPIC, motor_sub);

  // Enable power to the motor
  IOWrite(&VBUS_EN, 0);

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

void loop(void) {
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
