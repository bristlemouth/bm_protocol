#include "pa7ld.h"
#include "debug.h"

PA7LD::PA7LD(I2CInterface_t* i2cInterface, uint8_t address) {
  _interface = i2cInterface;
  _addr = static_cast<uint8_t>(address);
}

bool PA7LD::init() {
  // In here we can do the PROM reading and get
  // the Pmin and Pmax for later
  uint16_t cust0;
  uint16_t cust1;
  uint32_t scaling0;
  uint16_t scaling1;
  uint16_t scaling2;
  uint16_t scaling3;
  uint16_t scaling4;

  sendCommand(CUST_ID0, 1);
  vTaskDelay(2);
  uint8_t data[3] = {0};
  readData(data, sizeof(data));
  cust0 = data[1] << 8 | data[2];

  printf("cust0: %" PRIx16 "\n", cust0);

  sendCommand(CUST_ID1, 1);
  vTaskDelay(2);
  data[3] = {0};
  readData(data, sizeof(data));
  cust1 = data[1] << 8 | data[2];

  printf("cust1: %" PRIx16 "\n", cust1);

  sendCommand(SCALING0, 1);
  vTaskDelay(2);
  data[3] = {0};
  readData(data, sizeof(data));
  scaling0 = data[1] << 8 | data[2];

  printf("scaling0: %" PRIx32 "\n", scaling0);

  sendCommand(SCALING1, 1);
  vTaskDelay(2);
  data[3] = {0};
  readData(data, sizeof(data));
  scaling1 = data[1] << 8 | data[2];

  printf("scaling1: %" PRIx16 "\n", scaling1);

  sendCommand(SCALING2, 1);
  vTaskDelay(2);
  readData(data, sizeof(data));
  scaling2 = data[1] << 8 | data[2];

  printf("scaling2: %" PRIx16 "\n", scaling2);

  P_min = (float)(uint32_t(scaling1 << 16 | scaling2));
  printf("P_min: %f\n", P_min);
  P_min = 0;
  printf("P_min: %f\n", P_min);

  sendCommand(SCALING3, 1);
  vTaskDelay(2);
  readData(data, sizeof(data));
  scaling3 = data[1] << 8 | data[2];

  printf("scaling3: %" PRIx16 "\n", scaling3);

  sendCommand(SCALING4, 1);
  vTaskDelay(2);
  readData(data, sizeof(data));
  scaling4 = data[1] << 8 | data[2];

  printf("scaling4: %" PRIx16 "\n", scaling4);

  P_max = (float)(((uint32_t)scaling3 << 16 | scaling4));
  printf("P_max: %f\n", P_max);
  P_max = 10;
  printf("P_max: %f\n", P_max);

  return true;
}

bool PA7LD::reset() {
  // Do nothing for now
  return true;
}

bool PA7LD::readPTRaw(float &pressure, float &temperature) {
  pressure = 0.5;
  temperature = 0.5;
  bool rval = false;
  do {
    rval = sendCommand(REQUEST_MEASURMENT, 10);
    if (!rval) {
      break;
    }

    vTaskDelay(pdMS_TO_TICKS(10));

    uint8_t adc[5] = {0,0,0,0,0};
    // printf("sizeof adc: %d\n", sizeof(adc));
    rval = readData(adc, sizeof(adc));
    if (!rval) {
      break;
    }

    // for (uint8_t i = 0; i < sizeof(adc); i++) {
    //   printf("d: %" PRIx8 "\n", adc[i]);
    // }

    uint16_t press = ((uint16_t)adc[2] | ((uint16_t)adc[1] << 8));
    pressure = (float)(press - 16384) * (P_max - P_min) / 32768 + P_min;

    printf("press: %" PRIx16 "\n", press);

    uint16_t temp = ((uint16_t)adc[4] | ((uint16_t)adc[3] << 8));
    temperature = (float)(((temp >> 4) - 24) *  0.05) - 50;

    printf("temp: %" PRIx16 "\n", temp);

  } while (0);
  return rval;
}

bool PA7LD::checkPROM() {
  return true;
}

uint32_t PA7LD::signature() {
  return 1;
}

bool PA7LD::readPROM() {
  return true;
}

bool PA7LD::sendCommand(PA7LDCmd_t command, uint32_t timeoutMs) {
  (void) timeoutMs;
  return (writeBytes((uint8_t*)&command, sizeof(uint8_t)) == I2C_OK);
}

bool PA7LD::readData(uint8_t *data, size_t dataSize) {
  // How do we issue no command, just an address with the read bit set?
  // we can look at the i2c scan for how it does the address probing
  // and use the same method to just send the address? maybe?
  I2CResponse_t rval;

  do {
    // rval = writeBytes((uint8_t*)&command, sizeof(uint8_t));
    // if(rval != I2C_OK){
    //   printf("error writing to PA7LD: %d\n", rval);
    //   break;
    // }

    rval = readBytes(data, dataSize);
    // for (uint8_t i = 0; i < dataSize; i++) {
    //   printf("our bytes: %" PRIx8 "\n", data[i]);
    // }
    if(rval != I2C_OK){
      printf("error reading bytes from PA7LD: %d\n", rval);
    }

  } while (0);

  return (rval == I2C_OK);
}

bool PA7LD::getRawValue(uint32_t *value, uint32_t timeoutMs) {
  (void)value;
  (void)timeoutMs;
  bool rval = true;
  configASSERT(value != NULL);

  return rval;
}