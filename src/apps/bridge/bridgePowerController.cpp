#include "bridgePowerController.h"
#include "FreeRTOS.h"
#include "sensorController.h"
#include "app_pub_sub.h"
#include "bm_l2.h"
#include "bm_serial.h"
#include "bridgeLog.h"
#include "device_info.h"
#include "stm32_rtc.h"
#include "task.h"
#include "task_priorities.h"
#include "util.h"
#include <cinttypes>
#include <stdio.h>

BridgePowerController::BridgePowerController(IOPinHandle_t &BusPowerPin,
                                             uint32_t sampleIntervalMs,
                                             uint32_t sampleDurationMs,
                                             uint32_t subsampleIntervalMs,
                                             uint32_t subsampleDurationMs,
                                             bool subSamplingEnabled,
                                             bool powerControllerEnabled,
                                             uint32_t alignmentS)
    : _BusPowerPin(BusPowerPin), _powerControlEnabled(powerControllerEnabled),
      _sampleIntervalS(sampleIntervalMs/1000), _sampleDurationS(sampleDurationMs/1000),
      _subsampleIntervalS(subsampleIntervalMs/1000),
      _subsampleDurationS(subsampleDurationMs/1000), _sampleIntervalStartS(0),
      _subsampleIntervalStartS(0), _alignmentS(alignmentS), _rtcSet(false), _initDone(false),
      _subSamplingEnabled(subSamplingEnabled), _configError(false), _adin_handle(NULL) {
  if (_sampleIntervalS > MAX_SAMPLE_INTERVAL_S ||
      _sampleIntervalS < MIN_SAMPLE_INTERVAL_S) {
    printf("INVALID SAMPLE INTERVAL, using default.\n");
    _configError = true;
    _sampleIntervalS = DEFAULT_SAMPLE_INTERVAL_S;
  }
  if (_sampleDurationS > MAX_SAMPLE_DURATION_S ||
      _sampleDurationS < MIN_SAMPLE_DURATION_S) {
    printf("INVALID SAMPLE DURATION, using default.\n");
    _configError = true;
    _sampleDurationS = DEFAULT_SAMPLE_DURATION_S;
  }
  if (_subsampleIntervalS > MAX_SUBSAMPLE_INTERVAL_S ||
      _subsampleIntervalS < MIN_SUBSAMPLE_INTERVAL_S) {
    printf("INVALID SUBSAMPLE INTERVAL, using default.\n");
    _configError = true;
    _subsampleIntervalS = DEFAULT_SUBSAMPLE_INTERVAL_S;
  }
  if (_subsampleDurationS > MAX_SUBSAMPLE_DURATION_S ||
      _subsampleDurationS < MIN_SUBSAMPLE_DURATION_S) {
    printf("INVALID SUBSAMPLE DURATION, using default.\n");
    _configError = true;
    _subsampleDurationS = DEFAULT_SUBSAMPLE_DURATION_S;
  }
  if(_alignmentS > MAX_ALIGNMENT_S) {
    printf("INVALID ALIGNMENT, using default.\n");
    _configError = true;
    _alignmentS = DEFAULT_ALIGNMENT_S;
  }
  if(_sampleDurationS == _sampleIntervalS) {
    _configError = true;
    _powerControlEnabled = false;
  }
  if(_subsampleDurationS == _subsampleIntervalS) {
    _configError = true;
    _subSamplingEnabled = false;
  }
  _busPowerEventGroup = xEventGroupCreate();
  configASSERT(_busPowerEventGroup);
  BaseType_t rval =
      xTaskCreate(BridgePowerController::powerControllerRun, "Power Controller",
                  128 * 4, this, BRIDGE_POWER_TASK_PRIORITY, &_task_handle);
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
  if (enable) {
    _sampleIntervalStartS = getEpochS();
    _subSampleIntervalStartS = _sampleIntervalStartS;
    printf("Bridge power controller enabled\n");
  } else {
    printf("Bridge power controller disabled\n");
  }
  xTaskNotify(
      _task_handle, enable,
      eNoAction); // Notify the power controller task that the state has changed.
}

bool BridgePowerController::isPowerControlEnabled() {
  return _powerControlEnabled;
}

bool BridgePowerController::initPeriodElapsed() { return _initDone; }

bool BridgePowerController::waitForSignal(bool on, TickType_t ticks_to_wait) {
  bool rval = true;
  EventBits_t signal_to_wait_on =
      (on) ? BridgePowerController::ON : BridgePowerController::OFF;
  EventBits_t uxBits = xEventGroupWaitBits(
      _busPowerEventGroup, signal_to_wait_on, pdFALSE, pdFALSE, ticks_to_wait);
  if (uxBits & ~(signal_to_wait_on)) {
    rval = false;
  }
  return rval;
}

void BridgePowerController::powerBusAndSetSignal(bool on, bool notifyL2) {
  do {
    uint8_t enabled;
    if(IORead(&_BusPowerPin, &enabled) && (enabled == on)) {
      break;
    }
    EventBits_t signal_to_set =
      (on) ? BridgePowerController::ON : BridgePowerController::OFF;
    EventBits_t signal_to_clear =
        (on) ? BridgePowerController::OFF : BridgePowerController::ON;
    xEventGroupClearBits(_busPowerEventGroup, signal_to_clear);
    IOWrite(&_BusPowerPin, on);
    xEventGroupSetBits(_busPowerEventGroup, signal_to_set);
    if (notifyL2) {
      if (_adin_handle) {
        bm_l2_netif_set_power(_adin_handle, on);
      } else {
        if (getAdinDevice()) {
          bm_l2_netif_set_power(_adin_handle, on);
        }
      }
    }
    constexpr size_t bufsize = 25;
    static char buffer[bufsize];
    int len =
        snprintf(buffer, bufsize, "Bridge bus power: %d\n", static_cast<int>(on));
    BRIDGE_LOG_PRINTN(buffer, len);
  } while(0);
}

bool BridgePowerController::isBridgePowerOn(void) {
  uint8_t val;
  IORead(&_BusPowerPin, &val);
  return static_cast<bool>(val);
}

void BridgePowerController::subsampleEnable(bool enable) {
  _subSamplingEnabled = enable;
}

bool BridgePowerController::isSubsampleEnabled() { return _subSamplingEnabled; }

void BridgePowerController::_update(
    void) { // FIXME: Refactor this function to libStateMachine: https://github.com/wavespotter/bristlemouth/issues/379
  uint32_t time_to_sleep_ms = MIN_TASK_SLEEP_MS;
  do {
    if (!_initDone) { // Initializing
      BRIDGE_LOG_PRINT("Bridge State Init\n");
      powerBusAndSetSignal(
          true,
          false); // We start Bus on, no need to signal an eth up / power up event to l2 & adin
      vTaskDelay(
          INIT_POWER_ON_TIMEOUT_MS); // Set bus on for two minutes for init.
      if(_configError) {
        BRIDGE_LOG_PRINT("Bridge configuration error! Please check configs, using default.\n");
      }
      static constexpr size_t printBufSize = 200;
      char *printbuf = static_cast<char *>(pvPortMalloc(printBufSize));
      configASSERT(printbuf);
      size_t len = snprintf(printbuf, printBufSize,
                            "Sample enabled %d\n"
                            "Sample Duration: %" PRIu32 " s\n"
                            "Sample Interval: %" PRIu32 " s\n"
                            "Subsample enabled: %d \n"
                            "Subsample Duration: %" PRIu32 " s\n"
                            "Subsample Interval: %" PRIu32 " s\n"
                            "Alignment Interval: %" PRIu32 " s\n",
                            _powerControlEnabled, _sampleDurationS,
                            _sampleIntervalS, _subSamplingEnabled,
                            _subsampleDurationS, _subsampleIntervalS,
                            _alignmentS);
      BRIDGE_LOG_PRINTN(printbuf, len);
      vPortFree(printbuf);
      BRIDGE_LOG_PRINT("Bridge State Init Complete\n");
      checkAndUpdateRTC();
      _initDone = true;
    } else if (_powerControlEnabled && _rtcSet) { // Sampling Enabled
      uint32_t currentCycleS = getEpochS();
      uint32_t sampleTimeRemainingS = timeRemainingGeneric(_sampleIntervalStartS, currentCycleS, _sampleDurationS);
      if (sampleTimeRemainingS) {
        if (_subSamplingEnabled) { // Subsampling Enabled
          uint32_t subsampleTimeRemainingS = timeRemainingGeneric(_subSampleIntervalStartS,currentCycleS, _subsampleDurationS);
          if (subsampleTimeRemainingS) {
            BRIDGE_LOG_PRINT("Bridge State Subsample\n");
            powerBusAndSetSignal(true);
            time_to_sleep_ms = MAX(subsampleTimeRemainingS * 1000, MIN_TASK_SLEEP_MS);
            break;
          } else {
            BRIDGE_LOG_PRINT("Bridge State Subsampling Off\n");
            uint32_t nextSubsampleEpochS = _subsampleIntervalStartS + _subsampleIntervalS;
            _subSampleIntervalStartS = nextSubsampleEpochS;
            time_to_sleep_ms = (currentCycleS < nextSubsampleEpochS) ?
              MAX((nextSubsampleEpochS - currentCycleS) * 1000, MIN_TASK_SLEEP_MS) :
              MIN_TASK_SLEEP_MS;
            // Prevent bus thrash
            if (nextSubsampleEpochS > currentCycleS) {
              powerBusAndSetSignal(false);
            }
            break;
          }
        } else { // Subsampling disabled
          BRIDGE_LOG_PRINT("Bridge State Sample\n");
          powerBusAndSetSignal(true);
          time_to_sleep_ms = MAX(sampleTimeRemainingS * 1000, MIN_TASK_SLEEP_MS);
          break;
        }
      } else {
        BRIDGE_LOG_PRINT("Bridge State Sampling Off\n");
        uint32_t nextSampleEpochS = alignEpoch(_sampleIntervalStartS + _sampleIntervalS);
        _sampleIntervalStartS = nextSampleEpochS;
        _subsampleIntervalStartS = nextSampleEpochS;
        time_to_sleep_ms = (currentCycleS < nextSampleEpochS) ?
          MAX((nextSampleEpochS - currentCycleS) * 1000, MIN_TASK_SLEEP_MS) :
          MIN_TASK_SLEEP_MS;
         // Prevent bus thrash
        if (nextSampleEpochS > currentCycleS) {
          powerBusAndSetSignal(false);
        }
        xTaskNotify(sensor_controller_task_handle, AANDERAA_AGGREGATION_TIMER_BITS, eSetBits);
        break;
      }
    } else { // Sampling Not Enabled
      uint8_t enabled;
      if (!_powerControlEnabled && IORead(&_BusPowerPin, &enabled)) {
        if (!enabled) { // Turn the bus on if we've disabled the power manager.
          BRIDGE_LOG_PRINT("Bridge State Disabled - bus on\n");
          powerBusAndSetSignal(true);
        }
      } else if (!_rtcSet && _powerControlEnabled &&
                 IORead(&_BusPowerPin, &enabled)) {
        if (enabled) { // If our RTC is not set and we've enabled the power manager, we should disable the VBUS
          BRIDGE_LOG_PRINT("Bridge State Disabled - controller enabled, but "
                           "RTC is not yet set - bus off\n");
          powerBusAndSetSignal(false);
        }
      }
      checkAndUpdateRTC();
    }

  } while (0);
#ifndef CI_TEST
  uint32_t taskNotifyValue = 0;

  xTaskNotifyWait(pdFALSE, UINT32_MAX, &taskNotifyValue,
                  pdMS_TO_TICKS(time_to_sleep_ms));
#else // CI_TEST
  vTaskDelay(time_to_sleep_ms); // FIXME fix this in test.
#endif // CI_TEST
}

void BridgePowerController::powerControllerRun(void *arg) {
  while (true) {
    reinterpret_cast<BridgePowerController *>(arg)->_update();
  }
}

bool BridgePowerController::getAdinDevice() {
  bool rval = false;
  for (uint32_t port = 0; port < bm_l2_get_num_ports(); port++) {
    adin2111_DeviceHandle_t adin_handle;
    bm_netdev_type_t dev_type = BM_NETDEV_TYPE_NONE;
    uint32_t start_port_idx;
    if (bm_l2_get_device_handle(port, reinterpret_cast<void **>(&adin_handle),
                                &dev_type, &start_port_idx) &&
        (dev_type == BM_NETDEV_TYPE_ADIN2111)) {
      _adin_handle = adin_handle;
      rval = true;
      break;
    }
  }
  return rval;
}

void BridgePowerController::checkAndUpdateRTC() {
  if (isRTCSet() && !_rtcSet) {
    printf("Bridge Power Controller RTC is set.\n");
    _sampleIntervalStartS = getEpochS();
    _subsampleIntervalStartS = _sampleIntervalStartS;
    _rtcSet = true;
  }
}

uint32_t BridgePowerController::getEpochS() {
  RTCTimeAndDate_t datetime;
  rtcGet(&datetime);
  return static_cast<uint32_t>(rtcGetMicroSeconds(&datetime) * 1e-6);
}

uint32_t BridgePowerController::alignEpoch(uint32_t epochS) {
  uint32_t alignedEpoch = epochS;
  uint32_t alignmentDeltaS = 0;
  if(_alignmentS && epochS % _alignmentS != 0){
    alignmentDeltaS = (_alignmentS - (epochS % _alignmentS));
    alignedEpoch = epochS + alignmentDeltaS;
  }
  return alignedEpoch;
}
