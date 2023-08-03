#pragma once

#include "gpio.h"
#include "bsp.h"
#include "abstract_htu_sensor.h"
#include "abstract_pressure_sensor.h"
#include "tca9546a.h"
#include "ms5803.h"
#include "htu21d.h"
#include "bme280driver.h"
#include "ina232.h"

class Bristlefin {
  public:
  // I/O states for LED1 and LED2 elements.
  #define BF_LED_ON (0)
  #define BF_LED_OFF (1)

  // I2C Addresses
  static constexpr uint8_t I2C_ADDR_MOTE_THROUGH_POWER_MON = 0x41;
  static constexpr uint8_t I2C_ADDR_BMDK_LOAD_POWER_MON = 0x43;

  typedef enum {
    LED_OFF = 0,
    LED_GREEN,
    LED_RED,
    LED_YELLOW
  } led_state_e;

  typedef enum {
    BRISTLEFIN_V_UNKNOWN,
    BRISTLEFIN_V_1_1,
    BRISTLEFIN_V_1_2,
  } version_e;

  private:
    static constexpr uint8_t I2C_PROBE_MAX_TRIES = 5;

  public:  
    Bristlefin(MS5803 &ms5803,HTU21D& htu21d, Bme280 &bme280, TCA::TCA9546A& tca_mux, INA::INA232& ina232);
    bool sensorsInit();
    void setGpioDefault();
    // Power rail control functions
    void enableVbus();
    void disableVbus();
    void enable5V();
    void disable5V();
    void enableVout();
    void disableVout();
    void enable3V();
    void disable3V();

    // LED control functions
    void setLed(uint8_t led_num, led_state_e led_state);

    version_e getVersion();
    TCA::Channel_t getMuxChannel();
    void setMuxChannel(TCA::Channel_t ch);

  private:
    MS5803 &ms5803_;
    HTU21D &htu21d_;
    Bme280 &bme280_;
    TCA::TCA9546A& tca_mux_;
    INA::INA232& ina232_;
    version_e version_;
    TCA::Channel_t saved_mux_current_channel_;
};
