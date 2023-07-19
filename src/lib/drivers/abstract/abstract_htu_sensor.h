#pragma once
#include <stdint.h>

class AbstractHtu {
public:
    virtual bool init() = 0;
    virtual bool read(float &temperature, float &humidity) = 0;
    virtual float readTemperature() = 0;
    virtual float readHumidity() = 0;
};
