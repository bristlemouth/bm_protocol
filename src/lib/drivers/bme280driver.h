#pragma once
#include "bme280.h"
#include "abstract_i2c.h"
#include "abstract_pressure_sensor.h"
#include "abstract_htu_sensor.h"

class Bme280 : public AbstractI2C, public AbstractPressureSensor, public AbstractHtu {
public:
    Bme280(I2CInterface_t* i2cInterface, uint8_t address);
    bool init();
    bool read(float &temperature, float &humidity);
    float readTemperature();
    float readHumidity();
    bool readPressure(float &pressure);
    bool reset();
    bool readPTRaw(float &pressure, float &temperature);
    bool checkPROM();
    uint32_t signature();

public:
static constexpr float INVALID_TEMPERATURE = 999.0;
static constexpr float INVALID_HUMIDITY = 998.0;
static constexpr uint8_t I2C_ADDR = 0x76;

private:
    static BME280_INTF_RET_TYPE bme280_i2c_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t length, void *intf_ptr);
    static BME280_INTF_RET_TYPE bme280_i2c_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t length, void *intf_ptr);
    static void bme280_delay_us(uint32_t period, void *intf_ptr);

private:
    struct bme280_dev _dev;
    struct bme280_settings _settings;
    uint32_t _period;
};