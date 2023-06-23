#include "bridgePowerController.h"
#include "FreeRTOS.h"
#include "task.h"
#include "task_priorities.h"
#include "stm32_rtc.h"
#include "util.h"
#include <stdio.h>
#include "bm_serial.h"
#include "device_info.h"
#include "app_pub_sub.h"

BridgePowerController::BridgePowerController(IOPinHandle_t &BusPowerPin, uint32_t sampleIntervalMs, uint32_t sampleDurationMs, uint32_t subsampleIntervalMs, uint32_t subsampleDurationMs, bool subSamplingEnabled, bool powerControllerEnabled) :
_BusPowerPin(BusPowerPin), _powerControlEnabled(powerControllerEnabled),
_sampleIntervalMs(sampleIntervalMs), _sampleDurationMs(sampleDurationMs),
_subsampleIntervalMs(subsampleIntervalMs), _subsampleDurationMs(subsampleDurationMs),
_nextSampleIntervalTimeTimestamp(0), _nextSubSampleIntervalTimeTimestamp(0), _rtcSet(false), _initDone(false), _subSamplingEnabled(subSamplingEnabled) {
    if(_sampleIntervalMs > MAX_SAMPLE_INTERVAL_MS || _sampleIntervalMs < MIN_SAMPLE_INTERVAL_MS) {
        printf("INVALID SAMPLE INTERVAL, using default.\n");
        _sampleIntervalMs = DEFAULT_SAMPLE_INTERVAL_MS;
    }
    if(_sampleDurationMs > MAX_SAMPLE_DURATION_MS || _sampleDurationMs < MIN_SAMPLE_DURATION_MS) {
        printf("INVALID SAMPLE DURATION, using default.\n");
        _sampleDurationMs = DEFAULT_SAMPLE_DURATION_MS;
    }
    if(_subsampleIntervalMs > MAX_SUBSAMPLE_INTERVAL_MS || _subsampleIntervalMs < MIN_SUBSAMPLE_INTERVAL_MS) {
        printf("INVALID SUBSAMPLE INTERVAL, using default.\n");
        _subsampleIntervalMs = DEFAULT_SUBSAMPLE_INTERVAL_MS;
    }
    if(_subsampleDurationMs > MAX_SUBSAMPLE_DURATION_MS || _subsampleDurationMs < MIN_SUBSAMPLE_DURATION_MS) {
        printf("INVALID SUBSAMPLE DURATION, using default.\n");
        _subsampleDurationMs = DEFAULT_SUBSAMPLE_DURATION_MS;
    }
    _busPowerEventGroup = xEventGroupCreate();
    configASSERT(_busPowerEventGroup);
    BaseType_t rval = xTaskCreate(
                BridgePowerController::powerControllerRun,
                "Power Controller",
                128 * 4,
                this,
                BRIDGE_POWER_TASK_PRIORITY,
                &_task_handle);
    configASSERT(rval == pdTRUE);
}

/*!
* Enable / disable the power control. If the power control is disabled (and RTC is set) the bus is ON.
* If the the power control is disabled and RTC is not set, the bus is OFF
* If power control is enabled, bus control is set by the min/max control parameters.
* \param[in] : enable - true if power control is enabled, false if off.
*/
void BridgePowerController::powerControlEnable(bool enable) {
    _powerControlEnabled = enable;
    if(enable){
        uint32_t now;
        if(nowTimestampMs(now)){
            _nextSampleIntervalTimeTimestamp = now;
            _nextSubSampleIntervalTimeTimestamp = now;
        }
        printf("Bridge power controller enabled\n");
    } else {
        printf("Bridge power controller disabled\n");
    }
    xTaskNotify(_task_handle, enable, eNoAction); // Notify the power controller task that the state has changed.
}

bool BridgePowerController::isPowerControlEnabled() { return _powerControlEnabled; }

bool BridgePowerController::initPeriodElapsed() {return _initDone; }

bool BridgePowerController::waitForSignal(bool on, TickType_t ticks_to_wait) {
    bool rval = true;
    EventBits_t signal_to_wait_on = (on) ? BridgePowerController::ON : BridgePowerController::OFF;
    EventBits_t uxBits = xEventGroupWaitBits(
        _busPowerEventGroup,  
        signal_to_wait_on,
        pdFALSE,        
        pdFALSE,   
        ticks_to_wait );
    if(uxBits & ~(signal_to_wait_on)){
        rval = false;
    } 
    return rval;
}

void BridgePowerController::powerBusAndSetSignal(bool on) {
    EventBits_t signal_to_set = (on) ? BridgePowerController::ON : BridgePowerController::OFF;
    EventBits_t signal_to_clear = (on) ? BridgePowerController::OFF : BridgePowerController::ON;
    xEventGroupClearBits(_busPowerEventGroup, signal_to_clear);
    IOWrite(&_BusPowerPin, on);
    xEventGroupSetBits(_busPowerEventGroup, signal_to_set);
    static char buffer[25];
    int len = snprintf(buffer, 25, "Bridge bus power: %d", static_cast<int>(on));
    bm_serial_pub(getNodeId(), APP_PUB_SUB_BM_PRINTF_TOPIC, sizeof(APP_PUB_SUB_BM_PRINTF_TOPIC)-1, reinterpret_cast<const uint8_t *>(buffer) ,len, APP_PUB_SUB_BM_PRINTF_TYPE, APP_PUB_SUB_BM_PRINTF_VERSION);
}

bool BridgePowerController::isBridgePowerOn(void) {
    uint8_t val;
    IORead(&_BusPowerPin,&val);
    return static_cast<bool>(val);
}

void BridgePowerController::subSampleEnable(bool enable) {
    _subSamplingEnabled = enable;
}

bool BridgePowerController::isSubsampleEnabled() {
    return _subSamplingEnabled;
}

void BridgePowerController::_update(void) {
    uint32_t time_to_sleep_ms = MIN_TASK_SLEEP_MS;
    do {
        if (!_initDone) { // Initializing
            powerBusAndSetSignal(true);
            vTaskDelay(INIT_POWER_ON_TIMEOUT_MS); // Set bus on for two minutes for init.
            powerBusAndSetSignal(false);
            _initDone = true;
        } else if(_powerControlEnabled && _rtcSet) { // Sampling Enabled
            uint32_t now;
            if(!nowTimestampMs(now)){
                break;
            }
            if(now >= _nextSampleIntervalTimeTimestamp && now < _nextSampleIntervalTimeTimestamp + _sampleDurationMs) {
                if(_subSamplingEnabled){ // Subsampling Enabled
                    if(now >=_nextSubSampleIntervalTimeTimestamp && now < _nextSubSampleIntervalTimeTimestamp + _subsampleDurationMs){
                        powerBusAndSetSignal(true);
                        time_to_sleep_ms = MAX(_subsampleDurationMs,MIN_TASK_SLEEP_MS);
                        break;
                    } else if(now >= _nextSubSampleIntervalTimeTimestamp && now >=  _nextSubSampleIntervalTimeTimestamp + _subsampleDurationMs){
                        _nextSubSampleIntervalTimeTimestamp = now + (_subsampleIntervalMs - _subsampleDurationMs);
                        time_to_sleep_ms = MAX((_subsampleIntervalMs - _subsampleDurationMs), MIN_TASK_SLEEP_MS);
                        if(_nextSubSampleIntervalTimeTimestamp != now) { // Prevent bus thrash
                            powerBusAndSetSignal(false);
                        }
                        break;
                    }
                } else { // Subsampling disabled 
                    powerBusAndSetSignal(true);
                    time_to_sleep_ms = MAX(_sampleDurationMs, MIN_TASK_SLEEP_MS);
                    break;
                }
            } else if (now >= _nextSampleIntervalTimeTimestamp && now >= _nextSampleIntervalTimeTimestamp + _sampleDurationMs) {
                _nextSampleIntervalTimeTimestamp = now + (_sampleIntervalMs - _sampleDurationMs);
                _nextSubSampleIntervalTimeTimestamp = now + (_sampleIntervalMs - _sampleDurationMs);
                time_to_sleep_ms = MAX((_sampleIntervalMs - _sampleDurationMs), MIN_TASK_SLEEP_MS);
                if(_nextSampleIntervalTimeTimestamp != now) { // Prevent bus thrash
                    powerBusAndSetSignal(false);
                }
                break;
            }
        } else { // Sampling Not Enabled 
            uint8_t enabled;
            if(!_powerControlEnabled && IORead(&_BusPowerPin,&enabled)){ 
                if(!enabled) { // Turn the bus on if we've disabled the power manager.
                    printf("Bridge power controller disabled, turning bus on.\n");
                    powerBusAndSetSignal(true);
                }
            } else if (!_rtcSet && _powerControlEnabled && IORead(&_BusPowerPin,&enabled)) {
                if(enabled) { // If our RTC is not set and we've enabled the power manager, we should disable the VBUS
                    printf("Bridge power controller enabled, but RTC is not yet set, turning bus off.\n");
                    powerBusAndSetSignal(false);
                }
            }
            if(isRTCSet() && !_rtcSet){
                printf("Bridge Power Controller RTC is set.\n");
                uint32_t now;
                if(!nowTimestampMs(now)){
                    break;
                }
                _nextSampleIntervalTimeTimestamp = now;
                _nextSubSampleIntervalTimeTimestamp = now;
                _rtcSet = true;
            }
        }

    } while(0);
#ifndef CI_TEST
    uint32_t taskNotifyValue = 0;
    xTaskNotifyWait(pdFALSE, UINT32_MAX, &taskNotifyValue, pdMS_TO_TICKS(time_to_sleep_ms));
#else // CI_TEST
    vTaskDelay(time_to_sleep_ms); // FIXME fix this in test.
#endif // CI_TEST

}

bool BridgePowerController::nowTimestampMs(uint32_t &timestamp) {
    bool rval = false;
    do {
        RTCTimeAndDate_t datetime;
        if(!rtcGet(&datetime)) {
            break;
        }
        timestamp = utcFromDateTime(datetime.year, datetime.month, datetime.day, datetime.hour, datetime.minute, datetime.second) * 1000;
        rval = true;
    } while(0);
    return rval;
}

void BridgePowerController::powerControllerRun(void *arg) {
    while(true) {
        reinterpret_cast<BridgePowerController*>(arg)->_update();
    }
}

