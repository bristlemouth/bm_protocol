#include "bno085.h"
#include "uptime.h"
#include "debug.h"
#include "bsp.h"
#include "string.h"
#include "watchdog.h"

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
      _resetSeen(false),
      _reportInterval_us(100000) {
}

BmErr Bno085::init(sh2_EventCallback_t *eventCallback,
                  sh2_SensorCallback_t *sensorCallback) {
    _eventCallback = eventCallback;
    _sensorCallback = sensorCallback;
    printf("BNO085: Creating service task\n");

    return xTaskCreate(serviceTask, "BNO085", configMINIMAL_STACK_SIZE * 8,
                       this, BNO085_TASK_PRIORITY, &_serviceTaskHandle) == pdPASS ? BmOK : BmENOMEM;
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
    if (!waitForInt(1, pdMS_TO_TICKS(10))) {
        printf("BNO085: reset - INT did not deassert during reset\n");
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
    IOWrite(_rstPin, 1);
    if (!waitForInt(0, pdMS_TO_TICKS(500))) {
        printf("BNO085: reset - NO INT within 500ms\n");
        return false;
    }
    printf("BNO085: reset - INT asserted OK\n");
    return true;
}

bool Bno085::waitForInt(uint8_t level, TickType_t timeout) {
    TickType_t start = xTaskGetTickCount();
    uint8_t currentLevel = 0;
    do {
        IORead(_intPin, &currentLevel);
        if (currentLevel == level) return true;
        vTaskDelay(1);
    } while ((xTaskGetTickCount() - start) < timeout);
    return false;
}

void Bno085::serviceTask(void *arg) {
    printf("BNO085: serviceTask started, calling sh2_open\n");
    Bno085* instance = static_cast<Bno085*>(arg);

    // Setup HAL
    instance->_hal.open = Bno085::open;
    instance->_hal.close = Bno085::close;
    instance->_hal.read = Bno085::read;
    instance->_hal.write = Bno085::write;
    instance->_hal.getTimeUs = Bno085::getTimeUs;
    instance->configureSpiMode();
    int status = sh2_open(&instance->_hal, Bno085::eventHandler, instance);
    if (status != SH2_OK) {
        printf("BNO085: Failed to open sensor hub: %d\n", status);
        vTaskDelete(NULL);
        return;
    }

    // Set sensor callback
    if (instance->_sensorCallback) {
        sh2_setSensorCallback(instance->_sensorCallback, instance);
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

void Bno085::configureSpiMode() {
    _interface->handle->Init.CLKPolarity = SPI_POLARITY_HIGH;  // CPOL = 1
    _interface->handle->Init.CLKPhase    = SPI_PHASE_2EDGE;    // CPHA = 1  -> mode 3
    HAL_SPI_Init(_interface->handle);
}

BmErr Bno085::configureSensor(sh2_SensorId_t sensorId, uint32_t reportInterval_us) {
    if (!_initialized) {
        return BmENODEV;
    }

    sh2_SensorConfig_t config = {};
    config.reportInterval_us = reportInterval_us;

    int status = sh2_setSensorConfig(sensorId, &config);
    if (status != SH2_OK) {
        printf("BNO085: sh2_setSensorConfig(id=%d) failed: %d\n", sensorId, status);
        return BmEIO;
    }
    printf("BNO085: sh2_setSensorConfig(id=%d) OK\n", sensorId);
    return BmOK;
}

void Bno085::setReportInterval(uint32_t interval_us) { _reportInterval_us = interval_us; }

void Bno085::enableSensors() {
    const struct { sh2_SensorId_t id; const char *name; } wanted[] = {
        { SH2_ACCELEROMETER,             "accelerometer" },
        { SH2_GYROSCOPE_CALIBRATED,      "gyroscope" },
        { SH2_MAGNETIC_FIELD_CALIBRATED, "magnetometer" },
    };
    for (auto &w : wanted) {
        if (configureSensor(w.id, _reportInterval_us) == BmOK) {
            printf("BNO085: %s enabled @ %lu Hz\n", w.name, 1000000 / _reportInterval_us);
        } else {
            printf("BNO085: failed to enable %s\n", w.name);
        }
    }
}

int Bno085::open(sh2_Hal_t *self) {
    (void)self;
    return reinterpret_cast<Bno085*>(self)->resetSensor() ? SH2_OK : SH2_ERR;
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
    return reinterpret_cast<Bno085*>(self)->readBytes(pBuffer, len, t_us);
}

int Bno085::write(sh2_Hal_t *self, uint8_t *pBuffer, unsigned len) {
    (void)self;
    return reinterpret_cast<Bno085*>(self)->writeBytes(pBuffer, len);
}

uint32_t Bno085::getTimeUs(sh2_Hal_t *self) {
    (void)self;
    return uptimeGetMicroSeconds();
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
    if (t_us) *t_us = uptimeGetMicroSeconds();
    return cargoLength;
}

int Bno085::writeBytes(uint8_t *pBuffer, unsigned len) {
    watchdogFeed();
    IOWrite(_wakePin, 0);
    int written = -1;
    if (waitForInt(0, pdMS_TO_TICKS(1000)) &&
        spiTx(_interface, _csPin, len, pBuffer, 100) == SPI_OK) {
        written = (int)len;
    } else {
        printf("BNO085: TX failed (wake/SPI), len=%u\n", len);
    }
    IOWrite(_wakePin, 1);
    return written;
}
