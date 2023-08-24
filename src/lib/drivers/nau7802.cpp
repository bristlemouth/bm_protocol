#include "nau7802.h"
#include "FreeRTOS.h"
#include "debug.h"
#include "task.h"
#include "uptime.h"

NAU7802::NAU7802(I2CInterface_t *i2cInterface, uint8_t address)
    : _zeroOffset(-17678), _calibrationFactor(226.33) {
  _interface = i2cInterface;
  _addr = static_cast<uint8_t>(address);
}

bool NAU7802::init() {
  bool rval = true;
  return rval;
}

/*!
  Initialize NAU7802. Resets and reads PROM
  \return Will return false if sensor is not present or communicating
*/
bool NAU7802::begin() {
  printf("Init NAU7802\n");
  bool rval = true;
  {
    rval &= reset(); // Reset all registers
    printf("Post Reset rval: %u\n", rval);
    rval &= powerUp(); // Power on analog and digital sections of the scale
    printf("Post Power Up rval: %u\n", rval);
    rval &= setLDO(NAU7802_LDO_3V3); // Set LDO to 3.3V
    printf("Post setLDO rval: %u\n", rval);
    rval &= setGain(NAU7802_GAIN_128); // Set gain to 128
    printf("post setGain rval: %u\n", rval);
    rval &= setSampleRate(NAU7802_SPS_80); // Set samples per second to 10
    printf("post setSampleRate rval: %u\n", rval);
    // Turn off CLK_CHP. From 9.1 power on sequencing.
    rval &= setRegister(NAU7802_ADC, 0x30);
    printf("post setRegister rval: %u\n", rval);
    // Enable 330pF decoupling cap on chan 2. From 9.14 application circuit note.
    rval &= setBit(NAU7802_PGA_PWR_PGA_CAP_EN, NAU7802_PGA_PWR);
    printf("post setBit rval: %u\n", rval);

    // This call is disabled because it gives a really different result every time
    // (same every time on arduino). Risk of consequences but going ahead.
    // Re-caliablrate analog front end when we change gain, sample rate, or channel
    // rval &= calibrateAFE();
    // printf("post calibrateAFE rval: %u\n\n", rval);
  }
  return rval;
}

bool NAU7802::isConnected() {
  // Checking the revison code as a way to check for connection.
  // This is a little weird because getRevisionCode doesn't "fail" if the getRegister Command Fails.
  // In the failure case, I think it would just return whatever is in the memory addr for revisionCode, be it junk or otherwise.
  uint8_t revCode = getRevisionCode();
  if (revCode == 0x0F) {
    return (true);
  } else {
    return (false);
  }
}

void NAU7802::getInternalOffsetCal() {
  uint8_t first;
  uint8_t second;
  uint8_t third;
  getRegister(NAU7802_OCAL1_B0, &first);
  getRegister(NAU7802_OCAL1_B1, &second);
  getRegister(NAU7802_OCAL1_B2, &third);
  printf("byte zero %u\n", first);
  printf("byte one %u\n", second);
  printf("byte two %u\n", third);
}

// Returns true if Cycle Ready bit is set (conversion is complete)
bool NAU7802::available() {
  return (getBit(NAU7802_PU_CTRL_CR, NAU7802_PU_CTRL));
}

// Calibrate analog front end of system. Returns true if CAL_ERR bit is 0 (no error)
// Takes approximately 344ms to calibrate; wait up to 1000ms.
// It is recommended that the AFE be re-calibrated any time the gain, SPS, or channel number is changed.
bool NAU7802::calibrateAFE() {
  beginCalibrateAFE();
  return waitForCalibrateAFE(1000);
}

// Begin asynchronous calibration of the analog front end.
// Poll for completion with calAFEStatus() or wait with waitForCalibrateAFE()
void NAU7802::beginCalibrateAFE() { setBit(NAU7802_CTRL2_CALS, NAU7802_CTRL2); }

// Check calibration status.
NAU7802_Cal_Status NAU7802::calAFEStatus() {
  if (getBit(NAU7802_CTRL2_CALS, NAU7802_CTRL2)) {
    return NAU7802_CAL_IN_PROGRESS;
  }

  if (getBit(NAU7802_CTRL2_CAL_ERROR, NAU7802_CTRL2)) {
    return NAU7802_CAL_FAILURE;
  }

  // Calibration passed
  return NAU7802_CAL_SUCCESS;
}

// Wait for asynchronous AFE calibration to complete with optional timeout.
// If timeout is not specified (or set to 0), then wait indefinitely.
// Returns true if calibration completes succsfully, otherwise returns false.
bool NAU7802::waitForCalibrateAFE(uint32_t timeout_ms) {
  // Need to replace all calls to millis.
  uint32_t begin = uptimeGetMicroSeconds() / 1000;
  NAU7802_Cal_Status cal_ready;

  while ((cal_ready = calAFEStatus()) == NAU7802_CAL_IN_PROGRESS) {
    if ((timeout_ms > 0) &&
        (((uptimeGetMicroSeconds() / 1000) - begin) > timeout_ms)) {
      break;
    }
    vTaskDelay(pdMS_TO_TICKS(1));
  }

  if (cal_ready == NAU7802_CAL_SUCCESS) {
    return (true);
  }
  return (false);
}

// bool NAU7802::waitForCalibrateAFE(uint32_t timeout_ms)
// {
//   uint32_t begin = millis();
//   NAU7802_Cal_Status cal_ready;

//   while ((cal_ready = calAFEStatus()) == NAU7802_CAL_IN_PROGRESS)
//   {
//     if ((timeout_ms > 0) && ((millis() - begin) > timeout_ms))
//     {
//       break;
//     }
//     delay(1);
//   }

//   if (cal_ready == NAU7802_CAL_SUCCESS)
//   {
//     return (true);
//   }
//   return (false);
// }

// Set the readings per second
// 0, 20, 40, 80, and 320 samples per second is available
bool NAU7802::setSampleRate(uint8_t rate) {
  if (rate > 0b111)
    rate = 0b111; // Error check

  uint8_t value;
  getRegister(NAU7802_CTRL2, &value);
  value &= 0b10001111; // Clear CRS bits
  value |= rate << 4;  // Mask in new CRS bits

  bool result = setRegister(NAU7802_CTRL2, value);
  return result;
}

// Select between 1 and 2
bool NAU7802::setChannel(uint8_t channelNumber) {
  if (channelNumber == NAU7802_CHANNEL_1)
    return (clearBit(NAU7802_CTRL2_CHS, NAU7802_CTRL2)); // Channel 1 (default)
  else
    return (setBit(NAU7802_CTRL2_CHS, NAU7802_CTRL2)); // Channel 2
}

// Power up digital and analog sections of scale
bool NAU7802::powerUp() {
  setBit(NAU7802_PU_CTRL_PUD, NAU7802_PU_CTRL);
  setBit(NAU7802_PU_CTRL_PUA, NAU7802_PU_CTRL);

  // Wait for Power Up bit to be set - takes approximately 200us
  uint8_t counter = 0;
  while (1) {
    if (getBit(NAU7802_PU_CTRL_PUR, NAU7802_PU_CTRL) == true)
      break; // Good to go
    vTaskDelay(pdMS_TO_TICKS(1));
    if (counter++ > 100)
      return (false); // Error
  }
  return (true);
}

// Puts scale into low-power mode
bool NAU7802::powerDown() {
  clearBit(NAU7802_PU_CTRL_PUD, NAU7802_PU_CTRL);
  return (clearBit(NAU7802_PU_CTRL_PUA, NAU7802_PU_CTRL));
}

// Resets all registers to Power Of Defaults
bool NAU7802::reset() {
  setBit(NAU7802_PU_CTRL_RR, NAU7802_PU_CTRL); // Set RR
  vTaskDelay(pdMS_TO_TICKS(1));
  return (clearBit(NAU7802_PU_CTRL_RR,
                   NAU7802_PU_CTRL)); // Clear RR to leave reset state
}

// Set the onboard Low-Drop-Out voltage regulator to a given value
// 2.4, 2.7, 3.0, 3.3, 3.6, 3.9, 4.2, 4.5V are available
bool NAU7802::setLDO(uint8_t ldoValue) {
  if (ldoValue > 0b111)
    ldoValue = 0b111; // Error check
  // Set the value of the LDO

  uint8_t value;
  getRegister(NAU7802_CTRL1, &value);
  // uint8_t value = getRegister(NAU7802_CTRL1);
  value &= 0b11000111;    // Clear LDO bits
  value |= ldoValue << 3; // Mask in new LDO bits
  setRegister(NAU7802_CTRL1, value);
  // Enable the internal LDO
  return (setBit(NAU7802_PU_CTRL_AVDDS, NAU7802_PU_CTRL));
}

// Set the gain
// x1, 2, 4, 8, 16, 32, 64, 128 are avaialable
bool NAU7802::setGain(uint8_t gainValue) {
  if (gainValue > 0b111)
    gainValue = 0b111; // Error check

  uint8_t value;
  getRegister(NAU7802_CTRL1, &value);
  value &= 0b11111000; // Clear gain bits
  value |= gainValue;  // Mask in new bits

  bool result = setRegister(NAU7802_CTRL1, value);
  return result;
}

// Get the revision code of this IC
uint8_t NAU7802::getRevisionCode() {
  uint8_t revisionCode;
  getRegister(NAU7802_DEVICE_REV, &revisionCode);
  return (revisionCode & 0x0F);
}

// Returns 24-bit reading
// Assumes CR Cycle Ready bit (ADC conversion complete) has been checked to be 1
/*
I'm doing a check for the ready bit of the control register in the loadCellSampler.cpp
*/
int32_t NAU7802::getReading() {
  bool res;

  // get 24bit ADC value. Maybe need a delay in here based on the getRawValue from ms5803.cpp
  uint8_t adc[3] = {0, 0, 0};

  res = readData(NAU7802_ADCO_B2, adc, sizeof(adc));
  if (!res) {
    return (0); // error case. This is what the original library did.
  }
  uint32_t valueRaw = (uint32_t)adc[0] << 16; // MSB
  valueRaw |= (uint32_t)adc[1] << 8;          // MidSB
  valueRaw |= (uint32_t)adc[2];               // LSB

  int32_t valueShifted = (int32_t)(valueRaw << 8);
  int32_t value = (valueShifted >> 8);

  printf("adc0 - adc2: \t\t%u\t\t%u\t\t%u\n", adc[0], adc[1], adc[2]);
  return value;
  // The wire version just returns value.
}

// // Old code
// int32_t NAU7802::getReading()
// {
//   _i2cPort->beginTransmission(_deviceAddress);
//   _i2cPort->write(NAU7802_ADCO_B2);
//   if (_i2cPort->endTransmission() != 0)
//     return (false); // Sensor did not ACK

//   _i2cPort->requestFrom((uint8_t)_deviceAddress, (uint8_t)3);

//   if (_i2cPort->available())
//   {
//     uint32_t valueRaw = (uint32_t)_i2cPort->read() << 16; // MSB
//     valueRaw |= (uint32_t)_i2cPort->read() << 8;          // MidSB
//     valueRaw |= (uint32_t)_i2cPort->read();               // LSB

//     // the raw value coming from the ADC is a 24-bit number, so the sign bit now
//     // resides on bit 23 (0 is LSB) of the uint32_t container. By shifting the
//     // value to the left, I move the sign bit to the MSB of the uint32_t container.
//     // By casting to a signed int32_t container I now have properly recovered
//     // the sign of the original value
//     int32_t valueShifted = (int32_t)(valueRaw << 8);

//     // shift the number back right to recover its intended magnitude
//     int32_t value = (valueShifted >> 8);

//     return (value);
//   }

//   return (0); // Error
// }

// Return the average of a given number of readings
// Gives up after 1000ms so don't call this function to average 8 samples setup at 1Hz output (requires 8s)
int32_t NAU7802::getAverage(uint8_t averageAmount) {
  long total = 0;
  uint8_t samplesAquired = 0;

  unsigned long startTime = uptimeGetMicroSeconds() / 1000;
  while (1) {
    if (available() == true) {
      total += getReading();
      if (++samplesAquired == averageAmount)
        break; // All done
    }
    if (uptimeGetMicroSeconds() / 1000 - startTime > 1000)
      return (0); // Timeout - Bail with error
    vTaskDelay(1);
  }
  total /= averageAmount;

  return (total);
}

// Call when scale is setup, level, at running temperature, with nothing on it
void NAU7802::calculateZeroOffset(uint8_t averageAmount) {
  setZeroOffset(getAverage(averageAmount));
}

// Sets the internal variable. Useful for users who are loading values from NVM.
void NAU7802::setZeroOffset(int32_t newZeroOffset) {
  _zeroOffset = newZeroOffset;
}

int32_t NAU7802::getZeroOffset() { return (_zeroOffset); }
//

// Call after zeroing. Provide the float weight sitting on scale. Units do not matter.
void NAU7802::calculateCalibrationFactor(float weightOnScale,
                                         uint8_t averageAmount) {
  int32_t onScale = getAverage(averageAmount);
  float newCalFactor = (onScale - _zeroOffset) / (float)weightOnScale;
  setCalibrationFactor(newCalFactor);
}

// Pass a known calibration factor into library. Helpful if users is loading settings from NVM.
// If you don't know your cal factor, call setZeroOffset(), then calculateCalibrationFactor() with a known weight
void NAU7802::setCalibrationFactor(float newCalFactor) {
  _calibrationFactor = newCalFactor;
}

float NAU7802::getCalibrationFactor() { return (_calibrationFactor); }

// Returns the y of y = mx + b using the current weight on scale, the cal factor, and the offset.
float NAU7802::getWeight(bool allowNegativeWeights, uint8_t samplesToTake) {
  int32_t onScale = getAverage(samplesToTake);

  // Prevent the current reading from being less than zero offset
  // This happens when the scale is zero'd, unloaded, and the load cell reports a value slightly less than zero value
  // causing the weight to be negative or jump to millions of pounds
  if (allowNegativeWeights == false) {
    if (onScale < _zeroOffset)
      onScale = _zeroOffset; // Force reading to zero
  }

  float weight = (onScale - _zeroOffset) / _calibrationFactor;
  return (weight);
}

// Set Int pin to be high when data is ready (default)
bool NAU7802::setIntPolarityHigh() {
  // 0 = CRDY pin is high active (ready when 1)
  return (clearBit(NAU7802_CTRL1_CRP, NAU7802_CTRL1));
}

// Set Int pin to be low when data is ready
bool NAU7802::setIntPolarityLow() {
  return (setBit(NAU7802_CTRL1_CRP,
                 NAU7802_CTRL1)); // 1 = CRDY pin is low active (ready when 0)
}

// Mask & set a given bit within a register
bool NAU7802::setBit(uint8_t bitNumber, uint8_t registerAddress) {
  uint8_t value;
  getRegister(registerAddress, &value);
  value |= (1 << bitNumber); // Set this bit
  return (setRegister(registerAddress, value));
}

// Mask & clear a given bit within a register
bool NAU7802::clearBit(uint8_t bitNumber, uint8_t registerAddress) {
  uint8_t value;
  getRegister(registerAddress, &value);
  value &= ~(1 << bitNumber); // Set this bit
  return (setRegister(registerAddress, value));
}

// Return a given bit within a register
bool NAU7802::getBit(uint8_t bitNumber, uint8_t registerAddress) {
  uint8_t value;
  getRegister(registerAddress, &value);
  value &= (1 << bitNumber); // Clear all but this bit
  return (value);
}

// This was translated over from the ms5803 driver. I think it will do....
bool NAU7802::readData(uint8_t registerAddress, uint8_t *value,
                       size_t dataSize) {
  uint8_t regByte = registerAddress;
  I2CResponse_t rval;
  do {
    rval = writeBytes((uint8_t *)&regByte, sizeof(uint8_t), 100);
    if (rval != I2C_OK) {
      printf("error writing to NAU7802: %d\n", rval);
      break;
    }
    // vTaskDelay(pdMS_TO_TICKS(500));
    // adding this delay to maybe let the
    rval = readBytes(value, dataSize, 100);
    if (rval != I2C_OK) {
      printf("error reading bytes from NAU7802: %d\n", rval);
    }

  } while (0);
  return (rval == I2C_OK);
}

// Get contents of a register
bool NAU7802::getRegister(uint8_t registerAddress, uint8_t *value) {
  bool rval = false;
  uint8_t regByte = registerAddress;
  uint8_t regVal = 0;

  I2CResponse_t res;
  do {
    res = writeBytes((uint8_t *)&regByte, sizeof(regByte), 100);
    if (res != I2C_OK) {
      printf("error writing bytes: %d\n", res);
      break;
    }
    res = readBytes((uint8_t *)&regVal, sizeof(regVal), 100);
    if (res != I2C_OK) {
      printf("error reading bytes: %d\n", res);
      break;
    }

    if (value != NULL) {
      *value = regVal;
    }

    rval = true;
  } while (0);

  return rval;
}

// Send a given value to be written to given address
// Return true if successful
bool NAU7802::setRegister(uint8_t registerAddress, uint8_t value) {
  uint8_t bytes[] = {static_cast<uint8_t>(registerAddress),
                     static_cast<uint8_t>(value)};
  return (writeBytes(bytes, sizeof(bytes), 100) == I2C_OK);
}

// Ok, so I'm not going to be able to use calibrate scale to get a good measuremnt from this guy.
// I need to do things semi manually.
// Maybe as a first, check on the "ready" pin check and see if I can wait until it is.

// int32_t NAU7802::oldGetReading()
// {
//   _i2cPort->beginTransmission(_deviceAddress);
//   _i2cPort->write(NAU7802_ADCO_B2);
//   if (_i2cPort->endTransmission() != 0)
//     return (false); // Sensor did not ACK

//   _i2cPort->requestFrom((uint8_t)_deviceAddress, (uint8_t)3);

//   if (_i2cPort->available())
//   {
//     uint32_t valueRaw = (uint32_t)_i2cPort->read() << 16; // MSB
//     valueRaw |= (uint32_t)_i2cPort->read() << 8;          // MidSB
//     valueRaw |= (uint32_t)_i2cPort->read();               // LSB

//     // the raw value coming from the ADC is a 24-bit number, so the sign bit now
//     // resides on bit 23 (0 is LSB) of the uint32_t container. By shifting the
//     // value to the left, I move the sign bit to the MSB of the uint32_t container.
//     // By casting to a signed int32_t container I now have properly recovered
//     // the sign of the original value
//     int32_t valueShifted = (int32_t)(valueRaw << 8);

//     // shift the number back right to recover its intended magnitude
//     int32_t value = (valueShifted >> 8);

//     return (value);
//   }

//   return (0); // Error
// }
