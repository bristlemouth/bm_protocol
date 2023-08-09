#include <cinttypes>
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
#include "bridgeLog.h"
#include "bm_l2.h"

BridgePowerController::BridgePowerController(IOPinHandle_t &BusPowerPin, uint32_t sampleIntervalMs, uint32_t sampleDurationMs, uint32_t subsampleIntervalMs, uint32_t subsampleDurationMs, bool subSamplingEnabled, bool powerControllerEnabled) :
_BusPowerPin(BusPowerPin), _powerControlEnabled(powerControllerEnabled),
_sampleIntervalMs(sampleIntervalMs), _sampleDurationMs(sampleDurationMs),
_subsampleIntervalMs(subsampleIntervalMs), _subsampleDurationMs(subsampleDurationMs),
_sampleIntervalStartTicks(0), _subSampleIntervalStartTicks(0), _rtcSet(false), _initDone(false), _subSamplingEnabled(subSamplingEnabled), _adin_handle(NULL) {
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
        _sampleIntervalStartTicks = xTaskGetTickCount();
        _subSampleIntervalStartTicks = xTaskGetTickCount();
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

void BridgePowerController::powerBusAndSetSignal(bool on, bool notifyL2) {
    EventBits_t signal_to_set = (on) ? BridgePowerController::ON : BridgePowerController::OFF;
    EventBits_t signal_to_clear = (on) ? BridgePowerController::OFF : BridgePowerController::ON;
    xEventGroupClearBits(_busPowerEventGroup, signal_to_clear);
    IOWrite(&_BusPowerPin, on);
    xEventGroupSetBits(_busPowerEventGroup, signal_to_set);
    if(notifyL2) {
        if(_adin_handle){
            bm_l2_netif_set_power(_adin_handle, on);
        } else {
            if(getAdinDevice()){
                bm_l2_netif_set_power(_adin_handle, on);
            }
        }
    }
    constexpr size_t bufsize = 25;
    static char buffer[bufsize];
    int len = snprintf(buffer, bufsize, "Bridge bus power: %d\n", static_cast<int>(on));
    BRIDGE_LOG_PRINTN(buffer, len);
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

void BridgePowerController::_update(void) { // FIXME: Refactor this function to libStateMachine: https://github.com/wavespotter/bristlemouth/issues/379
    uint32_t time_to_sleep_ms = MIN_TASK_SLEEP_MS;
    do {
        if (!_initDone) { // Initializing
            BRIDGE_LOG_PRINT("Bridge State Init\n");
            powerBusAndSetSignal(true, false); // We start Bus on, no need to signal an eth up / power up event to l2 & adin
            vTaskDelay(INIT_POWER_ON_TIMEOUT_MS); // Set bus on for two minutes for init.
            powerBusAndSetSignal(false);
            static constexpr size_t printBufSize = 200;
            char* printbuf = static_cast<char*>(pvPortMalloc(printBufSize));
            configASSERT(printbuf);
            size_t len = snprintf(printbuf, printBufSize,
                "Sample enabled %d\n"
                "Sample Duration: %" PRIu32 " ms\n"
                "Sample Interval: %" PRIu32 " ms\n"
                "Subsample enabled: %d \n"
                "Subsample Duration: %" PRIu32 " ms\n"
                "Subsample Interval: %"  PRIu32 " ms\n",
                _powerControlEnabled,
                _sampleDurationMs,
                _sampleIntervalMs,
                _subSamplingEnabled,
                _subsampleDurationMs,
                _subsampleIntervalMs);
            BRIDGE_LOG_PRINTN(printbuf, len);
            vPortFree(printbuf);
            BRIDGE_LOG_PRINT("Bridge State Init Complete, powering off\n");
            _initDone = true;
        } else if(_powerControlEnabled && _rtcSet) { // Sampling Enabled
            uint32_t currentCycleTicks = xTaskGetTickCount();
            if(timeRemainingTicks(_sampleIntervalStartTicks, pdMS_TO_TICKS(_sampleDurationMs))) {
                if(_subSamplingEnabled){ // Subsampling Enabled
                    if(timeRemainingTicks(_subSampleIntervalStartTicks, pdMS_TO_TICKS(_subsampleDurationMs))){
                        BRIDGE_LOG_PRINT("Bridge State Subsample\n");
                        powerBusAndSetSignal(true);
                        time_to_sleep_ms = MAX(_subsampleDurationMs,MIN_TASK_SLEEP_MS);
                        break;
                    } else {
                        BRIDGE_LOG_PRINT("Bridge State Off\n");
                        _subSampleIntervalStartTicks = currentCycleTicks + (_subsampleIntervalMs - _subsampleDurationMs);
                        time_to_sleep_ms = MAX((_subsampleIntervalMs - _subsampleDurationMs), MIN_TASK_SLEEP_MS);
                        if(_subSampleIntervalStartTicks != currentCycleTicks) { // Prevent bus thrash
                            powerBusAndSetSignal(false);
                        }
                        break;
                    }
                } else { // Subsampling disabled 
                    BRIDGE_LOG_PRINT("Bridge State Sample\n");
                    powerBusAndSetSignal(true);
                    time_to_sleep_ms = MAX(_sampleDurationMs, MIN_TASK_SLEEP_MS);
                    break;
                }
            } else {
                BRIDGE_LOG_PRINT("Bridge State Off\n");
                _sampleIntervalStartTicks = currentCycleTicks + (_sampleIntervalMs - _sampleDurationMs);
                _subSampleIntervalStartTicks = currentCycleTicks + (_sampleIntervalMs - _sampleDurationMs);
                time_to_sleep_ms = MAX((_sampleIntervalMs - _sampleDurationMs), MIN_TASK_SLEEP_MS);
                if(_sampleIntervalStartTicks != currentCycleTicks) { // Prevent bus thrash
                    powerBusAndSetSignal(false);
                }
                break;
            }
        } else { // Sampling Not Enabled 
            uint8_t enabled;
            if(!_powerControlEnabled && IORead(&_BusPowerPin,&enabled)){ 
                if(!enabled) { // Turn the bus on if we've disabled the power manager.
                    BRIDGE_LOG_PRINT("Bridge State Disabled - bus on\n");
                    powerBusAndSetSignal(true);
                }
            } else if (!_rtcSet && _powerControlEnabled && IORead(&_BusPowerPin,&enabled)) {
                if(enabled) { // If our RTC is not set and we've enabled the power manager, we should disable the VBUS
                    BRIDGE_LOG_PRINT("Bridge State Disabled - controller enabled, but RTC is not yet set - bus off\n");
                    powerBusAndSetSignal(false);
                }
            }
            if(isRTCSet() && !_rtcSet){
                printf("Bridge Power Controller RTC is set.\n");
                _sampleIntervalStartTicks = xTaskGetTickCount();
                _subSampleIntervalStartTicks = xTaskGetTickCount();
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

void BridgePowerController::powerControllerRun(void *arg) {
    while(true) {
        reinterpret_cast<BridgePowerController*>(arg)->_update();
    }
}

bool BridgePowerController::getAdinDevice() {
    bool rval = false;
    for(uint32_t port = 0; port < bm_l2_get_num_ports(); port++) {
        adin2111_DeviceHandle_t adin_handle;
        bm_netdev_type_t dev_type = BM_NETDEV_TYPE_NONE;
        uint32_t start_port_idx;
        if(bm_l2_get_device_handle(port, reinterpret_cast<void **>(&adin_handle), &dev_type, &start_port_idx)
            && (dev_type == BM_NETDEV_TYPE_ADIN2111)) {
            _adin_handle = adin_handle;
            rval = true;
            break;
        }
    }
    return rval;
}
