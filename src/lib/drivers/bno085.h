#pragma once
extern "C" {
  #include "sh2.h"
  #include "sh2_hal.h"
  #include "sh2_err.h"
  #include "sh2_SensorValue.h"
}
#include "io.h"
#include "protected_spi.h"
#include "FreeRTOS.h"
#include "task.h"

class Bno085 {
public:
    // SPI bus + chip-select + INT + the three control pins (RST/BOOTN/WAKE)
    Bno085(SPIInterface_t *spi, IOPinHandle_t *csPin, IOPinHandle_t *intPin,
           IOPinHandle_t *rstPin, IOPinHandle_t *bootPin, IOPinHandle_t *wakePin);

    bool init(sh2_EventCallback_t *eventCallback,
              sh2_SensorCallback_t *sensorCallback);

    bool configureSensor(sh2_SensorId_t sensorId, uint32_t reportInterval_us);

    // HAL functions (called by sh2 library)
    static int open(sh2_Hal_t *self);
    static void close(sh2_Hal_t *self);
    static int read(sh2_Hal_t *self, uint8_t *pBuffer, unsigned len, uint32_t *t_us);
    static int write(sh2_Hal_t *self, uint8_t *pBuffer, unsigned len);
    static uint32_t getTimeUs(sh2_Hal_t *self);

private:
    static void serviceTask(void *arg);
    static bool intCallback(const void *pinHandle, uint8_t value, void *args);
    static void eventHandler(void *cookie, sh2_AsyncEvent_t *event); // reset recovery

    bool resetSensor();
    bool waitForIntLow(TickType_t timeout);
    int  readBytes(uint8_t *pBuffer, unsigned len, uint32_t *t_us);
    int  writeBytes(uint8_t *pBuffer, unsigned len);
    void enableSensors();

    SPIInterface_t* _interface;
    IOPinHandle_t *_csPin;
    IOPinHandle_t *_intPin;
    IOPinHandle_t *_rstPin;
    IOPinHandle_t *_bootPin;
    IOPinHandle_t *_wakePin;
    TaskHandle_t _serviceTaskHandle;

    sh2_EventCallback_t *_eventCallback;
    sh2_SensorCallback_t *_sensorCallback;

    bool _initialized;
    bool _resetSeen;

    // Static instance pointer for HAL callbacks
    static Bno085* _instance;
};
