#include "bno085.h"
#include "uptime.h"
#include "debug.h"
#include "bsp.h"
#include "string.h"
#include "watchdog.h"

// Initialize static instance pointer
Bno085* Bno085::_instance = nullptr;

Bno085::Bno085(SPIInterface_t *spi, IOPinHandle_t *csPin, IOPinHandle_t *intPin,
               IOPinHandle_t *rstPin, IOPinHandle_t *bootPin, IOPinHandle_t *wakePin)
    : _interface(spi),
      _csPin(csPin),
      _intPin(intPin),
      _rstPin(rstPin),
      _bootPin(bootPin),
      _wakePin(wakePin),
      _serviceTaskHandle(NULL),
      _eventCallback(NULL),
      _sensorCallback(NULL),
      _initialized(false),
      _resetSeen(false) {
         configASSERT(_instance == nullptr);
         _instance = this;
}

bool Bno085::init(sh2_EventCallback_t *eventCallback,
                  sh2_SensorCallback_t *sensorCallback) {
    _eventCallback = eventCallback;
    _sensorCallback = sensorCallback;
    printf("BNO085: Creating service task\n");

    return xTaskCreate(serviceTask, "BNO085", configMINIMAL_STACK_SIZE * 8,
                       this, 21, &_serviceTaskHandle) == pdPASS;
}

bool Bno085::intCallback(const void *pinHandle, uint8_t value, void *args) {
    (void)pinHandle; (void)value;
    Bno085 *self = static_cast<Bno085 *>(args);
    BaseType_t woken = pdFALSE;
    if (self->_serviceTaskHandle) vTaskNotifyGiveFromISR(self->_serviceTaskHandle, &woken);
    portYIELD_FROM_ISR(woken);
    return woken == pdTRUE;
}

// Hardware reset + SPI startup state. Straps latch on the RISING edge of RST, so the bus must
// be idle at that moment: WAKE and CS held high while releasing reset.
bool Bno085::resetSensor() {
    IOWrite(_bootPin, 1);
    IOWrite(_wakePin, 1);
    IOWrite(_csPin, 1);
    IOWrite(_rstPin, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    IOWrite(_rstPin, 1);
    if (!waitForIntLow(pdMS_TO_TICKS(500))) {
        printf("BNO085: reset - NO INT within 500ms\n");
        return false;
    }
    printf("BNO085: reset - INT asserted OK\n");
    return true;
}

bool Bno085::waitForIntLow(TickType_t timeout) {
    TickType_t start = xTaskGetTickCount();
    uint8_t level = 1;
    do {
        IORead(_intPin, &level);
        if (!level) return true;
        vTaskDelay(1);
    } while ((xTaskGetTickCount() - start) < timeout);
    return false;
}

void Bno085::serviceTask(void *arg) {
    printf("BNO085: serviceTask started, calling sh2_open\n");
    Bno085* instance = static_cast<Bno085*>(arg);

    // Setup HAL
    sh2_Hal_t hal = {};
    hal.open = Bno085::open;
    hal.close = Bno085::close;
    hal.read = Bno085::read;
    hal.write = Bno085::write;
    hal.getTimeUs = Bno085::getTimeUs;

    // Open sensor hub
    int status = sh2_open(&hal, Bno085::eventHandler, instance);
    if (status != SH2_OK) {
        printf("BNO085: Failed to open sensor hub: %d\n", status);
        vTaskDelete(NULL);
        return;
    }

    // Set sensor callback
    if (instance->_sensorCallback) {
        sh2_setSensorCallback(instance->_sensorCallback, NULL);
    }

    instance->_initialized = true;
    // BM_INT is EXTI0/falling in the BSP; hook our handler onto it.
    IORegisterCallback(instance->_intPin, Bno085::intCallback, instance);

    instance->_resetSeen = false;
    instance->enableSensors();

    // Main service loop
    while (1) {
        // Wait for notification from ISR (INT pin asserted)
        // Timeout after 100ms to periodically check even if no interrupt
        ulTaskNotifyTake(pdTRUE, 100);

        watchdogFeed();
        sh2_service();

        if (instance->_resetSeen) {
            instance->_resetSeen = false;
            printf("BNO085: reset detected - re-enabling sensors\n");
            instance->enableSensors();
        }
    }
}

bool Bno085::configureSensor(sh2_SensorId_t sensorId, uint32_t reportInterval_us) {
    if (!_initialized) {
        return false;
    }

    sh2_SensorConfig_t config = {};
    config.reportInterval_us = reportInterval_us;

    int status = sh2_setSensorConfig(sensorId, &config);
    if (status != SH2_OK) {
        printf("BNO085: sh2_setSensorConfig(id=%d) failed: %d\n", sensorId, status);
        return false;
    }
    printf("BNO085: sh2_setSensorConfig(id=%d) OK\n", sensorId);
    return true;
}

void Bno085::enableSensors() {
    const struct { sh2_SensorId_t id; const char *name; } wanted[] = {
        { SH2_ACCELEROMETER,             "accelerometer" },
        { SH2_GYROSCOPE_CALIBRATED,      "gyroscope" },
        { SH2_MAGNETIC_FIELD_CALIBRATED, "magnetometer" },
    };
    for (auto &w : wanted) {
        if (configureSensor(w.id, 100000)) {   // 10 Hz
            printf("BNO085: %s enabled @ 10 Hz\n", w.name);
        } else {
            printf("BNO085: failed to enable %s\n", w.name);
        }
    }
}

int Bno085::open(sh2_Hal_t *self) {
    (void)self;
    if (!_instance) return SH2_ERR;
    return _instance->resetSensor() ? SH2_OK : SH2_ERR;
}

void Bno085::close(sh2_Hal_t *self) {
    (void)self;
    // Nothing special needed
}

void Bno085::eventHandler(void *cookie, sh2_AsyncEvent_t *event) {
    Bno085 *self = static_cast<Bno085 *>(cookie);
    if (!self || !event) return;
    if (event->eventId == SH2_RESET) {
        self->_resetSeen = true;   // serviceTask re-applies sensor config
    }
    if (self->_eventCallback) {
        self->_eventCallback(self, event);   // forward to the app callback
    }
}

int Bno085::read(sh2_Hal_t *self, uint8_t *pBuffer, unsigned len, uint32_t *t_us) {
    (void)self;
    // Use the static instance pointer
    if (!_instance) {
        return 0;
    }
    return _instance->readBytes(pBuffer, len, t_us);
}

int Bno085::write(sh2_Hal_t *self, uint8_t *pBuffer, unsigned len) {
    (void)self;
    // Use the static instance pointer
    if (!_instance) {
        return 0;
    }
    return _instance->writeBytes(pBuffer, len);
}

uint32_t Bno085::getTimeUs(sh2_Hal_t *self) {
    (void)self;
    return uptimeGetMs() * 1000;
}

// Instance methods
int Bno085::readBytes(uint8_t *pBuffer, unsigned len, uint32_t *t_us) {
    watchdogFeed();
    uint8_t level = 1;
    IORead(_intPin, &level);
    if (level) {
        return 0;   // no data available; let sh2_service poll again
    }
    memset(pBuffer, 0, len);

    if (spiRx(_interface, _csPin, len, pBuffer, 100) != SPI_OK) {
        printf("BNO085: spiRx FAILED\n");
        return 0;
    }
    uint16_t cargoLength = (pBuffer[0] | (pBuffer[1] << 8)) & 0x7FFF;
    if (cargoLength < 4 || cargoLength > len) return 0;
    if (t_us) *t_us = uptimeGetMs() * 1000;
    return cargoLength;
}

int Bno085::writeBytes(uint8_t *pBuffer, unsigned len) {
    watchdogFeed();
    IOWrite(_wakePin, 0);
    int written = -1;
    if (waitForIntLow(pdMS_TO_TICKS(1000)) &&
        spiTx(_interface, _csPin, len, pBuffer, 100) == SPI_OK) {
        written = (int)len;
    } else {
        printf("BNO085: TX failed (wake/SPI), len=%u\n", len);
    }
    IOWrite(_wakePin, 1);
    return written;
}
