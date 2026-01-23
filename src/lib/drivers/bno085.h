#pragma once
extern "C" {
  #include "sh2.h"
  #include "sh2_hal.h"
  #include "sh2_err.h"
}
#include "abstract_i2c.h"

#define IMU_ADDR (0x4A)

// So I'm thinking of using this class as the provider of the functions that
// the sh2 library needs for its sh2.h api calls. Then we should be able
// to just create an instance of this class, create a struct that
// uses this class to create an sh2_hal struct, call the init
// and then we should be able to use the sh2 apis... atleast I think

// class Bno085 : public AbstractI2C {
// public:
//   Bno085(I2CInterface_t* i2cInterface, uint8_t address);
//   bool init();
//   int open(sh2_Hal_t *self);
//   void close(sh2_Hal_t *self);
//   int read(sh2_Hal_t *self, uint8_t *pBuffer, unsigned len, uint32_t *t_us);
//   int write(sh2_Hal_t *self, uint8_t *pBuffer, unsigned len);
//   uint32_t getTimeUs(sh2_Hal_t *self);

// public:
//   static constexpr uint8_t I2C_ADDR = 0x4A;


// private:
//   bool device_open;
//   sh2_Hal_t sh2_hal;
// };


#ifdef __cplusplus
extern "C" {
#endif

// C wrapper function to notify BNO085 from ISR
void bno085NotifyDataReadyFromISR(void);

#ifdef __cplusplus
}
#endif

class Bno085 {
public:
    Bno085(I2CInterface_t* i2cInterface, uint8_t address);

    bool init(sh2_EventCallback_t *eventCallback,
              sh2_SensorCallback_t *sensorCallback);

    bool configureSensor(sh2_SensorId_t sensorId, uint32_t reportInterval_us);

    // Called from ISR when INT pin asserts
    void notifyDataReady();

    // HAL functions (called by sh2 library)
    static int open(sh2_Hal_t *self);
    static void close(sh2_Hal_t *self);
    static int read(sh2_Hal_t *self, uint8_t *pBuffer, unsigned len, uint32_t *t_us);
    static int write(sh2_Hal_t *self, uint8_t *pBuffer, unsigned len);
    static uint32_t getTimeUs(sh2_Hal_t *self);
    // Add friend declaration to allow C wrapper to access private members
    friend void bno085NotifyDataReadyFromISR(void);

private:
    static void serviceTask(void *arg);

    // Instance methods for actual I2C operations
    int readBytes(uint8_t *pBuffer, unsigned len, uint32_t *t_us);
    int writeBytes(uint8_t *pBuffer, unsigned len);

    I2CInterface_t* _interface;
    uint8_t _addr;
    TaskHandle_t _serviceTaskHandle;
    SemaphoreHandle_t _i2cMutex;

    sh2_EventCallback_t *_eventCallback;
    sh2_SensorCallback_t *_sensorCallback;

    bool _initialized;

    // Static instance pointer for HAL callbacks
    static Bno085* _instance;
};
