#pragma once
#include "bno085.h"       // thin driver + sh2 types (sh2_SensorValue_t, decode, IDs)
#include "imu_types.h"    // neutral IMUReading / CompassReading (same dir)
#include "bm_os.h"        // BmSemaphore, bm_mutex/semaphore_*
#include "q.h"            // Q, QItem, q_create_static/enqueue/dequeue

// Adapter: presents the BNO085 through the same data_ready()/data_get() interface
// as MotionSampler, so the app can consume either IMU identically. The driver stays
// thin; all queueing/latching/neutralization lives here.
class Bno085Sampler {
public:
    Bno085Sampler(SPIInterface_t *spi, IOPinHandle_t *csPin, IOPinHandle_t *intPin,
                    IOPinHandle_t *rstPin, IOPinHandle_t *bootPin, IOPinHandle_t *wakePin);

    BmErr init();
    bool  data_ready(uint32_t timeout_ms = 50);
    BmErr data_get(IMUReading *reading);
    BmErr data_get(CompassReading *reading);

private:
    static void sensorCallback(void *cookie, sh2_SensorEvent_t *event);
    static void eventCallback(void *cookie, sh2_AsyncEvent_t *event);

    Bno085 _driver;
    static Bno085Sampler *_instance;

    static constexpr uint16_t IMU_QUEUE_COUNT = 32;
    static constexpr uint16_t MAG_QUEUE_COUNT = 32;
    Q _imuQueue;
    Q _magQueue;
    uint8_t _imuBuf[(sizeof(QItem) + sizeof(IMUReading)) * IMU_QUEUE_COUNT];
    uint8_t _magBuf[(sizeof(QItem) + sizeof(CompassReading)) * MAG_QUEUE_COUNT];
    BmSemaphore _queueMut;   // guards both queues (driver task <-> app task)
    BmSemaphore _imuSem;     // "IMU reading ready" -- mirrors LSM6DSV m_reading_sem

    IMUReading _latch;       // accel + latest gyro, emitted on each accel report
    bool _haveGyro;
};
