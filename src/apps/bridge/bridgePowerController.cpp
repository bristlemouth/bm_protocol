#include "bridgePowerController.h"
#include "FreeRTOS.h"
#include "app_pub_sub.h"
#include "app_util.h"
#include "bm_serial.h"
#include "bridgeLog.h"
#include "device_info.h"
#include "l2.h"
#include "sensorController.h"
#include "stm32_rtc.h"
#include "task.h"
#include "task_priorities.h"
#include <cinttypes>
#include <stdio.h>
#ifdef RAW_PRESSURE_ENABLE
#include "rbrPressureProcessor.h"
#endif // RAW_PRESSURE_ENABLE

BridgePowerController::BridgePowerController(
    IOPinHandle_t &BusPowerPin, uint32_t sampleIntervalMs, uint32_t sampleDurationMs,
    uint32_t subsampleIntervalMs, uint32_t subsampleDurationMs, bool subsamplingEnabled,
    bool powerControllerEnabled, uint32_t alignmentS, bool ticksSamplingEnabled)
    : _BusPowerPin(BusPowerPin), _powerControlEnabled(powerControllerEnabled),
      _sampleIntervalS(sampleIntervalMs / 1000), _sampleDurationS(sampleDurationMs / 1000),
      _subsampleIntervalS(subsampleIntervalMs / 1000),
      _subsampleDurationS(subsampleDurationMs / 1000), _sampleIntervalStartS(0),
      _subsampleIntervalStartS(0), _alignmentS(alignmentS),
      _ticksSamplingEnabled(ticksSamplingEnabled), _timebaseSet(false), _initDone(false),
      _subsamplingEnabled(subsamplingEnabled), _configError(false) {
  if (_sampleIntervalS > MAX_SAMPLE_INTERVAL_S || _sampleIntervalS < MIN_SAMPLE_INTERVAL_S) {
    printf("INVALID SAMPLE INTERVAL, using default.\n");
    _configError = true;
    _sampleIntervalS = DEFAULT_SAMPLE_INTERVAL_S;
  }
  if (_sampleDurationS > MAX_SAMPLE_DURATION_S || _sampleDurationS < MIN_SAMPLE_DURATION_S) {
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
  if (_alignmentS > MAX_ALIGNMENT_S) {
    printf("INVALID ALIGNMENT, using default.\n");
    _configError = true;
    _alignmentS = DEFAULT_ALIGNMENT_S;
  }
  if (_sampleDurationS == _sampleIntervalS) {
    _configError = true;
    _powerControlEnabled = false;
  }
  if (_subsampleDurationS == _subsampleIntervalS) {
    _configError = true;
    _subsamplingEnabled = false;
  }
  _busPowerEventGroup = xEventGroupCreate();
  configASSERT(_busPowerEventGroup);
  BaseType_t rval = xTaskCreate(BridgePowerController::powerControllerRun, "Power Controller",
                                128 * 4, this, BRIDGE_POWER_TASK_PRIORITY, &_task_handle);
  configASSERT(rval == pdTRUE);
}

/*!
* Enable / disable the power control. If the power control is disabled (and timebase is set) the bus is ON.
* If the the power control is disabled and timebase is not set, the bus is OFF
* If power control is enabled, bus control is set by the min/max control parameters.
* \param[in] : enable - true if power control is enabled, false if off.
*/
void BridgePowerController::powerControlEnable(bool enable) {
  _powerControlEnabled = enable;
  if (enable) {
    uint32_t currentCycleS = getCurrentTimeS();
    if (_sampleIntervalStartS < currentCycleS) {
      _sampleIntervalStartS =
          _alignNextInterval(currentCycleS, _sampleIntervalStartS, _sampleIntervalS);
      _subsampleIntervalStartS = _sampleIntervalStartS;
    }
    printf("Bridge power controller enabled\n");
  } else {
    printf("Bridge power controller disabled\n");
  }
  // Notify the power controller task that the state has changed.
  xTaskNotify(_task_handle, enable, eNoAction);
}

bool BridgePowerController::isPowerControlEnabled() { return _powerControlEnabled; }

bool BridgePowerController::initPeriodElapsed() { return _initDone; }

bool BridgePowerController::waitForSignal(bool on, TickType_t ticks_to_wait) {
  bool rval = true;
  EventBits_t signal_to_wait_on = (on) ? BridgePowerController::ON : BridgePowerController::OFF;
  EventBits_t uxBits = xEventGroupWaitBits(_busPowerEventGroup, signal_to_wait_on, pdFALSE,
                                           pdFALSE, ticks_to_wait);
  if (uxBits & ~(signal_to_wait_on)) {
    rval = false;
  }
  return rval;
}

void BridgePowerController::powerBusAndSetSignal(bool on, bool notifyL2) {
  do {
    uint8_t enabled;
    if (IORead(&_BusPowerPin, &enabled) && (enabled == on)) {
      break;
    }
    EventBits_t signal_to_set = (on) ? BridgePowerController::ON : BridgePowerController::OFF;
    EventBits_t signal_to_clear = (on) ? BridgePowerController::OFF : BridgePowerController::ON;
    xEventGroupClearBits(_busPowerEventGroup, signal_to_clear);
    IOWrite(&_BusPowerPin, on);
    xEventGroupSetBits(_busPowerEventGroup, signal_to_set);
    if (notifyL2) {
      bm_l2_netif_set_power(on);
    }
    bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_INFO, USE_HEADER, "Bridge bus power: %d\n",
                   static_cast<int>(on));
  } while (0);
}

bool BridgePowerController::isBridgePowerOn(void) {
  uint8_t val;
  IORead(&_BusPowerPin, &val);
  return static_cast<bool>(val);
}

void BridgePowerController::subsampleEnable(bool enable) { _subsamplingEnabled = enable; }

bool BridgePowerController::isSubsampleEnabled() { return _subsamplingEnabled; }

void BridgePowerController::stateLogPrintTarget(const char *state, uint32_t target) {
  bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_INFO, USE_HEADER,
                 "Bridge State %s until %" PRIu32 " %s seconds\n", state, target,
                 (_ticksSamplingEnabled) ? "uptime" : "epoch");
}

void BridgePowerController::_update(void) {
  uint32_t time_to_sleep_ms = MIN_TASK_SLEEP_MS;
  do {
    if (!_initDone) { // Initializing
      bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_INFO, USE_HEADER, "Bridge State Init\n");
      // We start Bus on, no need to signal an eth up / power up event to l2 & adin
      powerBusAndSetSignal(true, false);
      // Set bus on for two minutes for init.
      vTaskDelay(INIT_POWER_ON_TIMEOUT_MS);
      if (_configError) {
        bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_ERROR, USE_HEADER,
                       "Bridge configuration error! Please check configs, using default.\n");
      }
      bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_INFO, USE_HEADER,
                     "Sample enabled %d\n"
                     "Sample Duration: %" PRIu32 " s\n"
                     "Sample Interval: %" PRIu32 " s\n"
                     "Subsample enabled: %d \n"
                     "Subsample Duration: %" PRIu32 " s\n"
                     "Subsample Interval: %" PRIu32 " s\n"
                     "Alignment Interval: %" PRIu32 " s\n",
                     _powerControlEnabled, _sampleDurationS, _sampleIntervalS,
                     _subsamplingEnabled, _subsampleDurationS, _subsampleIntervalS,
                     _alignmentS);
      bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_INFO, USE_HEADER, "Using %s timebase\n",
                     _ticksSamplingEnabled ? "ticks" : "RTC");
      bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_INFO, USE_HEADER,
                     "Bridge State Init Complete\n");
      checkAndUpdateTimebase();
      uint32_t currentCycleS = getCurrentTimeS();
      if (_timebaseSet && _sampleIntervalStartS > currentCycleS) {
        time_to_sleep_ms =
            MAX((_sampleIntervalStartS - currentCycleS) * 1000, MIN_TASK_SLEEP_MS);
        // The default state until first sample interval depends
        // on whether bus power control is enabled.
        powerBusAndSetSignal(!_powerControlEnabled);
      }
      _initDone = true;
    } else if (_powerControlEnabled && _timebaseSet) { // Sampling Enabled
      uint32_t currentCycleS = getCurrentTimeS();
      if (currentCycleS < _sampleIntervalStartS) {
        time_to_sleep_ms = (_sampleIntervalStartS - currentCycleS) * 1000;
        bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_WARNING, USE_HEADER,
                       "Controller task woke early at %" PRIu32 ", will wait %" PRIu32 " ms\n",
                       currentCycleS, time_to_sleep_ms);
        break;
      }
      uint32_t sampleTimeRemainingS =
          timeRemainingGeneric(_sampleIntervalStartS, currentCycleS, _sampleDurationS);
      if (sampleTimeRemainingS) {
        if (_subsamplingEnabled) { // Subsampling Enabled
          if (currentCycleS < _subsampleIntervalStartS) {
            const uint32_t secondsUntilNextSubsample = _subsampleIntervalStartS - currentCycleS;
            const uint32_t timeToSleepS = MIN(sampleTimeRemainingS, secondsUntilNextSubsample);
            time_to_sleep_ms = MAX(timeToSleepS * 1000, MIN_TASK_SLEEP_MS);
            bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_WARNING, USE_HEADER,
                           "Controller task woke early at %" PRIu32 ", will wait %" PRIu32
                           " ms\n",
                           currentCycleS, time_to_sleep_ms);
            break;
          }
          uint32_t subsampleTimeRemainingS = timeRemainingGeneric(
              _subsampleIntervalStartS, currentCycleS, _subsampleDurationS);
          if (subsampleTimeRemainingS) {
            stateLogPrintTarget("Subsample", currentCycleS + subsampleTimeRemainingS);
            powerBusAndSetSignal(true);
            time_to_sleep_ms = MAX(subsampleTimeRemainingS * 1000, MIN_TASK_SLEEP_MS);
            break;
          } else {
            const uint32_t nextSubsampleEpochS = _subsampleIntervalStartS + _subsampleIntervalS;
            _subsampleIntervalStartS = nextSubsampleEpochS;
            const uint32_t secondsUntilNextSubsample =
                nextSubsampleEpochS > currentCycleS ? nextSubsampleEpochS - currentCycleS : 0;
            // Check whether this is the last subsample within a sample duration
            // If so, only sleep until sampling off, to align the next sample
            const uint32_t timeToSleepS = MIN(sampleTimeRemainingS, secondsUntilNextSubsample);
            stateLogPrintTarget("Subsampling Off", currentCycleS + timeToSleepS);
            time_to_sleep_ms = MAX(timeToSleepS * 1000, MIN_TASK_SLEEP_MS);
            // Prevent bus thrash
            if (nextSubsampleEpochS > currentCycleS) {
              powerBusAndSetSignal(false);
            }
            break;
          }
        } else { // Subsampling disabled
          stateLogPrintTarget("Sample", currentCycleS + sampleTimeRemainingS);
#ifdef RAW_PRESSURE_ENABLE
          if (!rbrPressureProcessorIsStarted()) {
            rbrPressureProcessorStart();
            bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_INFO, USE_HEADER,
                           "Started rbrPressureProcessor\n");
          }
#endif // RAW_PRESSURE_ENABLE
          powerBusAndSetSignal(true);
          time_to_sleep_ms = MAX(sampleTimeRemainingS * 1000, MIN_TASK_SLEEP_MS);
          break;
        }
      } else {
        uint32_t nextSampleEpochS =
            _alignNextInterval(currentCycleS, _sampleIntervalStartS, _sampleIntervalS);
        _sampleIntervalStartS = nextSampleEpochS;
        _subsampleIntervalStartS = nextSampleEpochS;
        stateLogPrintTarget("Sampling Off", nextSampleEpochS);
        time_to_sleep_ms =
            (currentCycleS < nextSampleEpochS)
                ? MAX((nextSampleEpochS - currentCycleS) * 1000, MIN_TASK_SLEEP_MS)
                : MIN_TASK_SLEEP_MS;
        // Prevent bus thrash
        if (nextSampleEpochS > currentCycleS) {
          powerBusAndSetSignal(false);
        }
        xTaskNotify(sensor_controller_task_handle, AGGREGATION_TIMER_BITS, eSetBits);
        break;
      }
    } else { // Sampling Not Enabled
      uint8_t enabled;
      if (!_powerControlEnabled && IORead(&_BusPowerPin, &enabled)) {
        if (!enabled) { // Turn the bus on if we've disabled the power manager.
          bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_INFO, USE_HEADER,
                         "Bridge State Disabled - bus on\n");
          powerBusAndSetSignal(true);
        }
      } else if (!_timebaseSet && _powerControlEnabled && IORead(&_BusPowerPin, &enabled)) {
        if (enabled) { // If our timebase is not set and we've enabled the power manager, we should disable the VBUS
          bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_INFO, USE_HEADER,
                         "Bridge State Disabled - controller enabled, but timebase is not yet "
                         "set - bus off\n");
          powerBusAndSetSignal(false);
        }

        // Align the first sample to UTC or tick offset when the timebase finally gets set
        checkAndUpdateTimebase();
        uint32_t currentCycleS = getCurrentTimeS();
        if (_timebaseSet && _sampleIntervalStartS > currentCycleS) {
          time_to_sleep_ms =
              MAX((_sampleIntervalStartS - currentCycleS) * 1000, MIN_TASK_SLEEP_MS);
        }
      }
      checkAndUpdateTimebase();
    }

  } while (0);

  if (time_to_sleep_ms > MIN_TASK_SLEEP_MS) {
    bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_INFO, USE_HEADER,
                   "Controller task will wait %" PRIu32 " ms\n", time_to_sleep_ms);
  }

#ifndef CI_TEST
  uint32_t taskNotifyValue = 0;
  xTaskNotifyWait(pdFALSE, UINT32_MAX, &taskNotifyValue, pdMS_TO_TICKS(time_to_sleep_ms));
#else  // CI_TEST
  vTaskDelay(time_to_sleep_ms); // FIXME fix this in test.
#endif // CI_TEST
}

void BridgePowerController::powerControllerRun(void *arg) {
  while (true) {
    reinterpret_cast<BridgePowerController *>(arg)->_update();
  }
}

void BridgePowerController::checkAndUpdateTimebase() {
  if ((isRTCSet() || _ticksSamplingEnabled) && !_timebaseSet) {
    printf("Bridge Power Controller timebase is set.\n");
    _sampleIntervalStartS =
        _alignNextInterval(getCurrentTimeS(), _sampleIntervalStartS, _sampleIntervalS);
    _subsampleIntervalStartS = _sampleIntervalStartS;
    _timebaseSet = true;
  }
}

uint32_t BridgePowerController::getCurrentTimeS() {
  if (_ticksSamplingEnabled) {
    return pdTICKS_TO_MS(xTaskGetTickCount()) * 1e-3;
  } else {
    RTCTimeAndDate_t datetime;
    rtcGet(&datetime);
    return static_cast<uint32_t>(rtcGetMicroSeconds(&datetime) * 1e-6);
  }
}

/*!
 * \brief Get next interval start time, possibly shifted to align with UTC.
 *
 * Given the current epoch time, the last interval start time, and the
 * duration of an interval, return the start time of the next interval
 * aligned to uptime or UTC according to the alignment config value.
 *
 * \param[in] nowEpochS - The current time in seconds since epoch.
 * \param[in] lastIntervalStartS - The start time of the last interval in seconds since epoch.
 * \param[in] sampleIntervalS - The duration of a sampling interval in seconds.
 * \return The start time of the next interval in seconds since epoch.
 */
uint32_t BridgePowerController::_alignNextInterval(uint32_t nowEpochS,
                                                   uint32_t lastIntervalStartS,
                                                   uint32_t sampleIntervalS) {
  if (!lastIntervalStartS) {
    // Prevent many loops from occurring and tripping watchdog
    lastIntervalStartS = nowEpochS - (nowEpochS % sampleIntervalS);
  }
  uint32_t alignedEpoch = lastIntervalStartS + sampleIntervalS;
  while (alignedEpoch < nowEpochS) {
    // If the aligned epoch is in the past, the timebase must have just jumped forward.
    // We need to add sample intervals until we reach the future.
    alignedEpoch += sampleIntervalS;
  }

  // If an alignment is configured, we need to align sampling intervals to uptime or UTC.
  if (_alignmentS != 0) {
    uint32_t remainder = alignedEpoch % _alignmentS;
    if (remainder != 0) {
      uint32_t adjustment = _alignmentS - remainder;
      // We only align forward because subtracting could take us into the past.
      // It would be possible to handle that situation,
      // but the code would get much more complicated.
      alignedEpoch += adjustment;
      bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_INFO, USE_HEADER,
                     "Aligning next sample interval to %s by delaying an additional %" PRIu32
                     " seconds to %" PRIu32 "\n",
                     (_ticksSamplingEnabled) ? "uptime" : "UTC", adjustment, alignedEpoch);
    }
  }

  return alignedEpoch;
}
