#include "bme280driver.h"
#include "FreeRTOS.h"
#include <stdlib.h>
#include <string.h>

Bme280::Bme280(I2CInterface_t* i2cInterface, uint8_t address) {
  _interface = i2cInterface;
  _addr = static_cast<uint8_t>(address);
}

bool Bme280::init() {
    bool rval = false;
    do {
        _dev.intf_ptr = this;
        _dev.read = bme280_i2c_read;
        _dev.write = bme280_i2c_write;
        _dev.intf = BME280_I2C_INTF;

        _dev.delay_us = bme280_delay_us;
        if(bme280_init(&_dev) !=  BME280_OK){
            break;
        }
        /* Always read the current settings before writing, especially when all the configuration is not modified */
        if(bme280_get_sensor_settings(&_settings, &_dev) != BME280_OK){
            break;
        }

        /* Configuring the over-sampling rate, filter coefficient and standby time */
        /* Overwrite the desired settings */
        _settings.filter = BME280_FILTER_COEFF_2;

        /* Over-sampling rate for humidity, temperature and pressure */
        _settings.osr_h = BME280_OVERSAMPLING_1X;
        _settings.osr_p = BME280_OVERSAMPLING_1X;
        _settings.osr_t = BME280_OVERSAMPLING_1X;

        /* Setting the standby time */
        _settings.standby_time = BME280_STANDBY_TIME_0_5_MS;

        if(bme280_set_sensor_settings(BME280_SEL_ALL_SETTINGS, &_settings, &_dev) != BME280_OK){
            break;
        }

        /* Always set the power mode after setting the configuration */
        if(bme280_set_sensor_mode(BME280_POWERMODE_NORMAL, &_dev) != BME280_OK){
            break;
        }

        /* Calculate measurement time in microseconds */
        if(bme280_cal_meas_delay(&_period, &_settings) != BME280_OK){
            break;
        }
        rval = true;
    } while(0);
    return rval;
}

bool Bme280::read(float &temperature, float &humidity) {
    bool rval = false;
    do {
        temperature = readTemperature();
        if(temperature == INVALID_TEMPERATURE){
            break;
        }
        humidity = readHumidity();
        if(humidity == INVALID_HUMIDITY){
            break;
        }
        rval = true;
    } while(0);
    return rval;
}

// Read temperature in deg C
float Bme280::readTemperature() {
    float rval = INVALID_TEMPERATURE;
    struct bme280_data comp_data;
    do {
        if(bme280_get_sensor_data(BME280_TEMP, &comp_data, &_dev) != BME280_OK){
            break;
        }
        rval = comp_data.temperature;
    } while(0);
    return rval;
}

// Read humidity in %RH
float Bme280::readHumidity() {
    float rval = INVALID_HUMIDITY;
    struct bme280_data comp_data;
    do {
        if(bme280_get_sensor_data(BME280_HUM, &comp_data, &_dev) != BME280_OK){
            break;
        }
        rval = comp_data.humidity;
    } while(0);
    return rval;
}

// Read pressure in hPa/Millibar 
bool Bme280::readPressure(float &pressure) {
    bool rval = false;
    struct bme280_data comp_data;
    do {
        if(bme280_get_sensor_data(BME280_PRESS, &comp_data, &_dev) != BME280_OK){
            break;
        }
        pressure = comp_data.pressure / 100.0;
        rval = true;
    } while(0);
    return rval;
}

bool Bme280::reset() {
    bool rval = false;
    do {
        if(bme280_soft_reset(&_dev) != BME280_OK) {
            break;
        }
        rval = true;
    } while(0);
    return rval;
}

bool Bme280::readPTRaw(float &pressure, float &temperature) {
    bool rval = false;
    do {
        if(!readPressure(pressure)){
            break;
        }
        temperature = readTemperature();
        if(temperature == INVALID_TEMPERATURE){
            break;
        }
        rval = true;
    } while(0);
    return rval;
}

bool Bme280::checkPROM() {
    return true; // Not relevant for BME280, dummy implementation for compatibility with AbstractPressureSensor parent class.
}

uint32_t Bme280::signature() {
    return _dev.calib_data.dig_t1 ^ _dev.calib_data.dig_t2 ^ _dev.calib_data.dig_t3 ^
    _dev.calib_data.dig_p1 ^ _dev.calib_data.dig_p2 ^ _dev.calib_data.dig_p3  ^
    _dev.calib_data.dig_p4 ^ _dev.calib_data.dig_p5 ^ _dev.calib_data.dig_p6 ^ 
    _dev.calib_data.dig_p7 ^ _dev.calib_data.dig_p8 ^ _dev.calib_data.dig_p9 ^ 
    _dev.calib_data.dig_h1 ^ _dev.calib_data.dig_h2 ^ _dev.calib_data.dig_h3 ^ 
    _dev.calib_data.dig_h4 ^ _dev.calib_data.dig_h5 ^  _dev.calib_data.dig_h6;
}

BME280_INTF_RET_TYPE Bme280::bme280_i2c_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t length, void *intf_ptr)
{
    int8_t res = BME280_OK;
    do {
        Bme280* inst = static_cast<Bme280*>(intf_ptr);
        res = inst->writeBytes(&reg_addr, sizeof(reg_addr));
        if(res != I2C_OK){
            break;
        }
        res = inst->readBytes(reg_data,length);
    } while(0);
    return res;
}

BME280_INTF_RET_TYPE Bme280::bme280_i2c_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t length, void *intf_ptr)
{
    int8_t res = BME280_OK;
    size_t bufsize = length + sizeof(reg_addr);
    uint8_t* databuf = static_cast<uint8_t*>(pvPortMalloc(bufsize));
    configASSERT(databuf);
    databuf[0] = reg_addr;
    memcpy(databuf + sizeof(reg_addr), reg_data, length);
    Bme280* inst = static_cast<Bme280*>(intf_ptr);
    res = inst->writeBytes(databuf, bufsize);
    vPortFree(databuf);
    return res;
}

/*!
 * Delay function map to COINES platform
 */
void Bme280::bme280_delay_us(uint32_t period, void *intf_ptr)
{
    (void) intf_ptr;
    vTaskDelay(period);
}
