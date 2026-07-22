#include "bno085Sampler.h"
#include "uptime.h"

Bno085Sampler *Bno085Sampler::_instance = nullptr;
Bno085Sampler::Bno085Sampler(SPIInterface_t *spi, IOPinHandle_t *csPin, IOPinHandle_t *intPin,
                             IOPinHandle_t *rstPin, IOPinHandle_t *bootPin, IOPinHandle_t *wakePin)
    : _driver(spi, csPin, intPin, rstPin, bootPin, wakePin),
      _queueMut(NULL),
      _imuSem(NULL),
      _haveGyro(false),
      _haveMag(false) {
        configASSERT(_instance == nullptr);   // one BNO on this board -> one sampler
        _instance = this;
}

BmErr Bno085Sampler::init() {
    // Create sync primitives + queues BEFORE starting the driver, since a callback
    // can fire as soon as the driver's service task runs.
    _queueMut = bm_mutex_create();
    _imuSem   = bm_semaphore_create();
    if (!_queueMut || !_imuSem) {
        return BmENOMEM;
    }
    if (q_create_static(&_imuQueue, _imuBuf, sizeof(_imuBuf)) != BmOK) {
        return BmENOMEM;
    }
    _latch = {};
    _haveGyro = false;
    _driver.setReportInterval(1000000 / _cfg.sample_rate_hz);   // convert Hz to us

    return _driver.init(eventCallback, sensorCallback);
}

// Runs on the driver's service task (during sh2_service). BNO delivers accel/gyro/mag
// as separate reports (accel ~2x the others), so we latch the latest gyro and emit a
// combined IMUReading anchored on each accel. Native BNO units: m/s^2, rad/s, uT.
void Bno085Sampler::sensorCallback(void *cookie, sh2_SensorEvent_t *event) {
    (void)cookie;
    Bno085Sampler *self = _instance;
    if (!self || !event) return;

    sh2_SensorValue_t v;
    if (sh2_decodeSensorEvent(&v, event) != SH2_OK) return;

    uint64_t ns = (uint64_t)uptimeGetMicroSeconds() * 1000ULL;

    switch (v.sensorId) {
    case SH2_ACCELEROMETER:
        self->_latch.ns  = ns;
        self->_latch.acc = { v.un.accelerometer.x, v.un.accelerometer.y, v.un.accelerometer.z };
        if (self->_haveGyro && self->_haveMag) {   // wait for at least one gyro/mag sample before emitting
        bm_semaphore_take(self->_queueMut, BM_MAX_DELAY_UINT32);
        q_enqueue(&self->_imuQueue, &self->_latch, sizeof(IMUReading));
        bm_semaphore_give(self->_queueMut);
        bm_semaphore_give(self->_imuSem);
        }
        break;

    case SH2_GYROSCOPE_CALIBRATED:
        self->_latch.gyro = { v.un.gyroscope.x, v.un.gyroscope.y, v.un.gyroscope.z };
        self->_haveGyro = true;
        break;

    case SH2_MAGNETIC_FIELD_CALIBRATED: {
        self->_latch.mag = { v.un.magneticField.x, v.un.magneticField.y, v.un.magneticField.z };
        self->_haveMag = true;
        break;
    }
    default:
        break;
    }
}

void Bno085Sampler::eventCallback(void *cookie, sh2_AsyncEvent_t *event) {
    (void)cookie;
    Bno085Sampler *self = _instance;
    if (!self || !event) return;
    // The driver re-enables its reports on reset; we just drop the stale gyro latch
    // so a post-reset reading never pairs a fresh accel with a pre-reset gyro.
    if (event->eventId == SH2_RESET) {
        self->_haveGyro = false;
        self->_haveMag = false;
    }
}

bool Bno085Sampler::data_ready(uint32_t timeout_ms) {
    if (!_imuSem) {
        bm_delay(timeout_ms);
        return false;
    }
    return bm_semaphore_take(_imuSem, timeout_ms) == BmOK;
}

BmErr Bno085Sampler::data_get(IMUReading *reading) {
    if (!reading)   return BmEINVAL;
    if (!_queueMut) return BmENODEV;
    bm_semaphore_take(_queueMut, BM_MAX_DELAY_UINT32);
    BmErr err = q_dequeue(&_imuQueue, reading, sizeof(IMUReading));
    bm_semaphore_give(_queueMut);
    return err;
}

void Bno085Sampler::set_cfg(Bno085SamplerConfig cfg) { _cfg = cfg; }

Bno085SamplerConfig bno085_sampler_get_default_config(void) {
    return Bno085SamplerConfig{ .sample_rate_hz = 10 };
}

// Parallel to motion_sampler_add, but no task is created here: the Bno085 driver
// spawns its own service task inside init(), so the "add" just runs init directly.
BmErr bno085_sampler_add(Bno085Sampler *sampler) {
    if (!sampler) return BmEINVAL;
    return sampler->init();
}
