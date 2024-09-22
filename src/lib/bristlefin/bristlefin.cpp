#include "bristlefin.h"
#include "debug.h"
#include "FreeRTOS.h"

void pressureSamplerInit(AbstractPressureSensor *sensor); // src/lib/sensor_sampler/pressureSampler.cpp
void htuSamplerInit(AbstractHtu *sensor); // src/lib/sensor_sampler/htuSampler.cpp

Bristlefin::Bristlefin(MS5803 &ms5803,HTU21D& htu21d, Bme280 &bme280, TCA::TCA9546A& tca_mux, INA::INA232& ina232) :
ms5803_(ms5803),
htu21d_(htu21d),
bme280_(bme280),
tca_mux_(tca_mux),
ina232_(ina232),
version_(Bristlefin::BRISTLEFIN_V_UNKNOWN),
saved_mux_current_channel_(TCA::CH_UNKNOWN){}

bool Bristlefin::sensorsInit() {
  bool rval = false;
  do{
    if(tca_mux_.init()){
      // Bristlefin has I2C channels 1 and 2 connected. Enable both.
      setMuxChannel(TCA::CH_1 | TCA::CH_2);
    } else {
      break;
    }

    uint8_t try_count = 0;
    I2CResponse_t ret = bme280_.probe();
    while(ret != I2C_OK && try_count++ < I2C_PROBE_MAX_TRIES){
      ret = bme280_.probe();
      vTaskDelay(pdMS_TO_TICKS(10));
    }
    if(ret == I2C_OK) {
      // Initialize temperature/humidity
      htuSamplerInit(&bme280_);

      // Initialize Barometer sampling
      pressureSamplerInit(&bme280_);
      version_ = Bristlefin::BRISTLEFIN_V_1_2;
    } else {
      // Initialize temperature/humidity
      htuSamplerInit(&htu21d_);

      // Initialize Barometer sampling
      pressureSamplerInit(&ms5803_);
      version_ = Bristlefin::BRISTLEFIN_V_1_1;
    }
    rval = true;
  } while(0);
  return rval;
}

// Power rail control functions
void Bristlefin::enableVbus() {
  IOWrite(&VBUS_BF_EN, 0); // 0 enables, 1 disables. Needed for VOUT and 5V.
}

void Bristlefin::disableVbus() {
  IOWrite(&VBUS_BF_EN, 1); // 0 enables, 1 disables. Needed for VOUT and 5V.
}

void Bristlefin::enable5V() {
  IOWrite(&BF_5V_EN, 0); // 0 enables, 1 disables. Needed for SDI12 and RS485.
}

void Bristlefin::disable5V() {
  IOWrite(&BF_5V_EN, 1); // 0 enables, 1 disables. Needed for SDI12 and RS485.
}

void Bristlefin::enableVout() {
  IOWrite(&BF_PL_BUCK_EN, 0); // 0 enables, 1 disables. Vout
}

void Bristlefin::disableVout() {
  IOWrite(&BF_PL_BUCK_EN, 1); // 0 enables, 1 disables. Vout
}

void Bristlefin::enable3V() {
  IOWrite(&BF_IMU_RST, 1); // https://github.com/wavespotter/bristlemouth/issues/422 - Drive IMU RST high to avoid backpowering scenario.
  IOWrite(&BF_3V3_EN, 1); // 1 enables, 0 disables. Needed for I2C and I/O control.
  setMuxChannel(saved_mux_current_channel_); // Can't talk to mux until 3v3 is enabled
}

void Bristlefin::disable3V() {
  IOWrite(&BF_IMU_RST, 0); // https://github.com/wavespotter/bristlemouth/issues/422 - Drive IMU RST low to avoid consuming xtra power.
  saved_mux_current_channel_ = getMuxChannel();
  setMuxChannel(TCA::CH_NONE); // Lowest power state for I2C mux
  IOWrite(&BF_3V3_EN, 0); // 1 enables, 0 disables. Needed for I2C and I/O control.
}

void Bristlefin::setLed(uint8_t led_num, led_state_e led_state) {
  // Error handling: check if led_num is valid
  if (led_num < 1 || led_num > 2) {
    printf("Invalid LED number. Choose between 1 and 2.\n");
    return;
  }

  // Determine which LED state to set based on led_state
  switch (led_state) {
    case LED_GREEN:
      if (led_num == 1) {
        IOWrite(&BF_LED_G1, BF_LED_ON);
        IOWrite(&BF_LED_R1, BF_LED_OFF);
      }
      else {
        IOWrite(&BF_LED_G2, BF_LED_ON);
        IOWrite(&BF_LED_R2, BF_LED_OFF);
      }
      break;

    case LED_RED:
      if (led_num == 1) {
        IOWrite(&BF_LED_G1, BF_LED_OFF);
        IOWrite(&BF_LED_R1, BF_LED_ON);
      }
      else {
        IOWrite(&BF_LED_G2, BF_LED_OFF);
        IOWrite(&BF_LED_R2, BF_LED_ON);
      }
      break;

    case LED_YELLOW:
      if (led_num == 1) {
        IOWrite(&BF_LED_G1, BF_LED_ON);
        IOWrite(&BF_LED_R1, BF_LED_ON);
      }
      else {
        IOWrite(&BF_LED_G2, BF_LED_ON);
        IOWrite(&BF_LED_R2, BF_LED_ON);
      }
      break;

    case LED_OFF:
      if (led_num == 1) {
        IOWrite(&BF_LED_G1, BF_LED_OFF);
        IOWrite(&BF_LED_R1, BF_LED_OFF);
      }
      else {
        IOWrite(&BF_LED_G2, BF_LED_OFF);
        IOWrite(&BF_LED_R2, BF_LED_OFF);
      }
      break;
  }
}

void Bristlefin::setRS485HalfDuplex() {
  // Set the RS485 transceiver to Half-Duplex (HFIO = 1)
  // Per MAX3089ECSD Datasheet
  IOWrite(&BF_HFIO, 1);
}

void Bristlefin::setRS485FullDuplex() {
  // Set the RS485 transceiver to Full-Duplex (HFIO = 0)
  // Per MAX3089ECSD Datasheet
  IOWrite(&BF_HFIO, 0);
}

void Bristlefin::setRS485Tx() {
  /// For more details see driver datasheet: https://www.notion.so/bristlemouth/Internal-Bristlemouth-VEMCO-Receiver-Integration-043988c56f84454db1a31381fd825beb?pvs=4#85bbbf0ea1cf43a69e7624427f5e2356
  // Disable the Rx-enable so we don't listen to ourselves (RE).
  // Enable the Tx-enable (DE).
  IOWrite(&GPIO1, 1); // GPIO1 is connected to DE active HIGH
  IOWrite(&GPIO2, 1); // GPIO2 is connected to RE active LOW
  // A 62 us setup delay is required,
  //  but 1ms is the smallest thread-safe non-blocking RTOS delay we have.
  vTaskDelay(1);
}

void Bristlefin::setRS485Rx() {
  /// For more details see driver datasheet: https://www.notion.so/bristlemouth/Internal-Bristlemouth-VEMCO-Receiver-Integration-043988c56f84454db1a31381fd825beb?pvs=4#85bbbf0ea1cf43a69e7624427f5e2356
  // Enable the Rx-receiver.
  // Disable the Tx-driver (DE). This is to save power, and may be required for some devices.
  IOWrite(&GPIO1, 0); // GPIO1 is connected to DE active HIGH
  IOWrite(&GPIO2, 0); // GPIO2 is connected to RE active LOW
}


Bristlefin::version_e Bristlefin::getVersion() {return version_;}

TCA::Channel_t Bristlefin::getMuxChannel() {
  TCA::Channel_t ch = TCA::CH_UNKNOWN;
  tca_mux_.getChannel(ch);
  return ch;
}

void Bristlefin::setMuxChannel(TCA::Channel_t ch) {
  saved_mux_current_channel_ = ch;
  tca_mux_.setChannel(ch);
}

void Bristlefin::setGpioDefault() {
  setLed(1, Bristlefin::LED_OFF);
  setLed(2, Bristlefin::LED_OFF);
  disable5V();
  enable3V();
  disableVbus();
  disableVout();
  IOWrite(&BF_HFIO, 0);
  IOWrite(&BF_SDI12_OE, 0);
}
