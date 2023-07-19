#pragma once
#include <stdint.h>

class AbstractPressureSensor {
public:
    virtual bool init() = 0;
    virtual bool reset() = 0 ;
    virtual bool readPTRaw(float &pressure, float &temperature) = 0;
    virtual bool checkPROM() = 0;
    virtual uint32_t signature() = 0;
};
