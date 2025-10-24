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
#include "uptime.h"
#include <cinttypes>
#include <stdio.h>
#ifdef RAW_PRESSURE_ENABLE
#include "rbrPressureProcessor.h"
#endif // RAW_PRESSURE_ENABLE

BridgePowerController::BridgePowerController(const Config &config)
    : _BusLoadSwitchEnablePin(config.BusLoadSwitchEnablePin),
      _BoostEnablePin(config.BoostEnablePin),
      _powerControlEnabled(config.powerControllerEnabled), _powerControlContinuousMode(false),
      _sampleIntervalS(config.sampleIntervalMs / 1000),
      _sampleDurationS(config.sampleDurationMs / 1000),
      _subsampleIntervalS(config.subsampleIntervalMs / 1000),
      _subsampleDurationS(config.subsampleDurationMs / 1000), _sampleIntervalStartS(0),
      _subsampleIntervalStartS(0), _alignmentS(config.alignmentS),
      _ticksSamplingEnabled(config.ticksSamplingEnabled), _timebaseSet(false), _initDone(false),
      _subsamplingEnabled(config.subsamplingEnabled), _configError(false) {
  validateConfig();
  _busPowerEventGroup = xEventGroupCreate();
  configASSERT(_busPowerEventGroup);
  BaseType_t rval = xTaskCreate(BridgePowerController::powerControllerRun, "Power Controller",
                                128 * 4, this, BRIDGE_POWER_TASK_PRIORITY, &_task_handle);
  configASSERT(rval == pdTRUE);

  configASSERT(power_info_service_init(powerInfoStatsCb, this) == BmOK);
}

void BridgePowerController::validateConfig(void) {
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

  // Truncate duration if they are greater than intervals
  if (_sampleDurationS > _sampleIntervalS) {
    _sampleDurationS = _sampleIntervalS;
  }
  if (_subsampleIntervalS > _sampleIntervalS) {
    _subsampleIntervalS = _sampleIntervalS;
    _subsamplingEnabled = false;
  }
  if (_subsampleDurationS > _subsampleIntervalS) {
    _subsampleDurationS = _subsampleIntervalS;
  }

  if (_powerControlEnabled && _sampleDurationS == _sampleIntervalS) {
    _powerControlContinuousMode = true;
  }
  if (_subsampleDurationS == _subsampleIntervalS) {
    _subsamplingEnabled = false;
  }
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

/*!
 * \brief Controls bridge bus power with proper sequencing and event signaling.
 *
 * Power-up sequence (when busOn=true):
 * 1. Turn on boost converter first
 * 2. Wait for capacitors to charge (CAPACITOR_CHARGE_DELAY_MS)
 * 3. Turn on load switch to enable bus power
 *
 * Power-down sequence (when busOn=false):
 * 1. Turn off load switch first to disconnect the load
 * 2. Turn off boost converter (saves ~3mW)
 *
 * The function also:
 * - Checks current pin state to avoid unnecessary operations
 * - Sets/clears event group bits to signal power state changes to waiting tasks
 * - Optionally notifies L2 network layer of power state changes
 * - Disables unused port 2 during power-up for power savings
 * - Logs power state changes for debugging
 *
 * \param[in] busOn - true to turn on bus power, false to turn off
 * \param[in] notifyL2 - true to notify L2 network layer of power changes (default: true)
 *
 * \note The boost converter must be turned on before the load switch to allow
 *       capacitor charging time. See PR#335 for detailed analysis.
 * \note Port 2 is automatically disabled during power-up as it's unused on bridge
 *       hardware and disabling provides power savings.
 */
void BridgePowerController::setBusPowerAndSetSignal(bool busOn, bool notifyL2) {
  do {
    uint8_t enabled;
    if (IORead(&_BusLoadSwitchEnablePin, &enabled) && (enabled == busOn)) {
      break;
    }
    EventBits_t signal_to_set =
        (busOn) ? BridgePowerController::ON : BridgePowerController::OFF;
    EventBits_t signal_to_clear =
        (busOn) ? BridgePowerController::OFF : BridgePowerController::ON;
    xEventGroupClearBits(_busPowerEventGroup, signal_to_clear);
    if (busOn) {
      // Turn on boost first, allow time for capacitors to charge, turn on the load switch. (See PR#335 for more details)
      IOWrite(&_BoostEnablePin, static_cast<uint8_t>(busOn));
      vTaskDelay(CAPACITOR_CHARGE_DELAY_MS);
      IOWrite(&_BusLoadSwitchEnablePin, static_cast<uint8_t>(busOn));
    } else {
      // Turn off the load switch first, then turn off the boost converter.
      IOWrite(&_BusLoadSwitchEnablePin, static_cast<uint8_t>(busOn));
      // Turning the boost converter off saves ~3mW.
      IOWrite(&_BoostEnablePin, static_cast<uint8_t>(busOn));
    }
    xEventGroupSetBits(_busPowerEventGroup, signal_to_set);
    if (notifyL2) {
      bm_l2_netif_set_power(busOn);
      // Must turn port 2 off if powering up the network,
      // port 2 is not used on the bridge and disabling the port will provide
      // power savings
      if (busOn) {
        bm_l2_netif_enable_disable_port(2, false);
      }
    }
    bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_INFO, USE_HEADER, "Bridge bus power: %d\n",
                   static_cast<int>(busOn));
  } while (0);
}

bool BridgePowerController::isBridgePowerOn(void) {
  uint8_t val;
  IORead(&_BusLoadSwitchEnablePin, &val);
  return static_cast<bool>(val);
}

void BridgePowerController::subsampleEnable(bool enable) { _subsamplingEnabled = enable; }

bool BridgePowerController::isSubsampleEnabled() { return _subsamplingEnabled; }

void BridgePowerController::stateLogPrintTarget(const char *state, uint32_t target) {
  bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_INFO, USE_HEADER,
                 "Bridge State %s until %" PRIu32 " %s seconds\n", state, target,
                 (_ticksSamplingEnabled) ? "uptime" : "epoch");
}

/*!
 @brief Handles Initialization State Of Bridge Power Controller

 @details During this state the bridge is powered on for INIT_POWER_ON_TIMEOUT_MS.
          Following this time period, if RTC is set, the bus power will be turned
          off if the _powerControllerEnabled variable is set to true and the
          _powerControlContinuousMode is false, then a delay will occur until
          the first sampling interval will begin.

 @return the time the task should sleep until the next action is to be taken.
 */
uint32_t BridgePowerController::handleInitState(void) {
  uint32_t time_to_sleep_ms = MIN_TASK_SLEEP_MS;

  bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_INFO, USE_HEADER, "Bridge State Init\n");
  // We start Bus on, no need to signal an eth up / power up event to l2 & adin
  setBusPowerAndSetSignal(true, false);
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
                 _powerControlEnabled, _sampleDurationS, _sampleIntervalS, _subsamplingEnabled,
                 _subsampleDurationS, _subsampleIntervalS, _alignmentS);
  bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_INFO, USE_HEADER, "Using %s timebase\n",
                 _ticksSamplingEnabled ? "ticks" : "RTC");
  bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_INFO, USE_HEADER,
                 "Bridge State Init Complete\n");
  checkAndUpdateTimebase();
  uint32_t currentCycleS = getCurrentTimeS();
  if (_timebaseSet && _sampleIntervalStartS > currentCycleS) {
    time_to_sleep_ms = MAX((_sampleIntervalStartS - currentCycleS) * 1000, MIN_TASK_SLEEP_MS);
    // The default state until first sample interval depends
    // on whether bus power control is enabled.
    bool power = !_powerControlEnabled || _powerControlContinuousMode;
    setBusPowerAndSetSignal(power);
  }
  _initDone = true;

  return time_to_sleep_ms;
}

/*!
 @brief Handles Sub Sampling Of The Bridge Power Controller

 @details Subsampling is very similar to how sampling works on the bridge power
          controller, but operates only during the duration period. During the
          subsampling duration power will be enabled on the Bristlemouth
          network and the difference between the subsample interval and the
          duration is when the power will be disabled. Data will only be
          collected from sensors on the network when the power is enabled.

 @param sampleTimeRemainingS Tick or epoch time remaining before the next
                             sample is started is seconds
 @param currentCycleS Current tick or epoch time in second

 @return the time the task should sleep until the next action is to be taken.
 */
uint32_t BridgePowerController::handleSubsamplingState(uint32_t sampleTimeRemainingS,
                                                       uint32_t currentCycleS) {
  uint32_t time_to_sleep_ms = MIN_TASK_SLEEP_MS;

  if (currentCycleS < _subsampleIntervalStartS) {
    const uint32_t secondsUntilNextSubsample = _subsampleIntervalStartS - currentCycleS;
    const uint32_t timeToSleepS = MIN(sampleTimeRemainingS, secondsUntilNextSubsample);
    time_to_sleep_ms = MAX(timeToSleepS * 1000, MIN_TASK_SLEEP_MS);
    bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_WARNING, USE_HEADER,
                   "Controller task woke early at %" PRIu32 ", will wait %" PRIu32 " ms\n",
                   currentCycleS, time_to_sleep_ms);
    return time_to_sleep_ms;
  }

  uint32_t subsampleTimeRemainingS =
      timeRemainingGeneric(_subsampleIntervalStartS, currentCycleS, _subsampleDurationS);
  if (!subsampleTimeRemainingS) {
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
      setBusPowerAndSetSignal(false);
    }
  } else {
    stateLogPrintTarget("Subsample", currentCycleS + subsampleTimeRemainingS);
    setBusPowerAndSetSignal(true);
    time_to_sleep_ms = MAX(subsampleTimeRemainingS * 1000, MIN_TASK_SLEEP_MS);
  }

  return time_to_sleep_ms;
}

/*!
 @brief Handles Sampling Of The Bridge Power Controller

 @details This state of the bridge power controller's task will determine if 
          the sampling duration or interval has been completed and perform
          necessary actions. The sampling duration is the amount of time the
          Bristlemouth network's power is on and data is being collected and
          aggregated by the bridge. The difference between the sampling
          interval a duration is the amount of time the Bristlemouth network
          will be off. Aggregated data reports occur at the end of a sampling
          duration.

          If at the beginning of a sampling duration, the task will
          determine if subsampling is enabled, and if so handle subsampling
          accordingly, else the task will enable the power to the Bristlemouth
          network and sleep until the end of the duration.

          If at the end of a sampling duration, the amount of time the power
          will be disabled is calculated and the power to the Bristlemouth
          network will be turned off if the bridge power controller is not
          operating in continuous mode.

 @return the time the task should sleep until the next action is to be taken.
 */
uint32_t BridgePowerController::handleSampleState(void) {
  uint32_t time_to_sleep_ms = MIN_TASK_SLEEP_MS;
  uint32_t currentCycleS = getCurrentTimeS();

  if (currentCycleS < _sampleIntervalStartS) {
    time_to_sleep_ms = (_sampleIntervalStartS - currentCycleS) * 1000;
    bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_WARNING, USE_HEADER,
                   "Controller task woke early at %" PRIu32 ", will wait %" PRIu32 " ms\n",
                   currentCycleS, time_to_sleep_ms);
    return time_to_sleep_ms;
  }

  uint32_t sampleTimeRemainingS =
      timeRemainingGeneric(_sampleIntervalStartS, currentCycleS, _sampleDurationS);
  if (!sampleTimeRemainingS) {
    uint32_t nextSampleEpochS =
        _alignNextInterval(currentCycleS, _sampleIntervalStartS, _sampleIntervalS);
    _sampleIntervalStartS = nextSampleEpochS;
    _subsampleIntervalStartS = nextSampleEpochS;
    time_to_sleep_ms = (currentCycleS < nextSampleEpochS)
                           ? MAX((nextSampleEpochS - currentCycleS) * 1000, MIN_TASK_SLEEP_MS)
                           : MIN_TASK_SLEEP_MS;
    // Prevent bus thrash
    if (nextSampleEpochS > currentCycleS && !_powerControlContinuousMode) {
      stateLogPrintTarget("Sampling Off", nextSampleEpochS);
      setBusPowerAndSetSignal(false);
    }

    // Notify sensor controller that an aggregation report should occur
    xTaskNotify(sensor_controller_task_handle, AGGREGATION_TIMER_BITS, eSetBits);
  } else if (_subsamplingEnabled) {
    time_to_sleep_ms = handleSubsamplingState(sampleTimeRemainingS, currentCycleS);
  } else {
    stateLogPrintTarget("Sample", currentCycleS + sampleTimeRemainingS);
#ifdef RAW_PRESSURE_ENABLE
    if (!rbrPressureProcessorIsStarted()) {
      rbrPressureProcessorStart();
      bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_INFO, USE_HEADER,
                     "Started rbrPressureProcessor\n");
    }
#endif // RAW_PRESSURE_ENABLE
    setBusPowerAndSetSignal(true);
    time_to_sleep_ms = MAX(sampleTimeRemainingS * 1000, MIN_TASK_SLEEP_MS);
  }

  return time_to_sleep_ms;
}

void BridgePowerController::_update(void) {
  uint32_t time_to_sleep_ms = MIN_TASK_SLEEP_MS;

  if (!_initDone) {
    time_to_sleep_ms = handleInitState();
  } else if (_powerControlEnabled && _timebaseSet) {
    time_to_sleep_ms = handleSampleState();
  } else {
    uint8_t enabled;
    time_to_sleep_ms = TIMEBASE_NOT_SET_SLEEP_MS;
    if (!_powerControlEnabled && IORead(&_BusLoadSwitchEnablePin, &enabled)) {
      if (!enabled) { // Turn the bus on if we've disabled the power manager.
        bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_INFO, USE_HEADER,
                       "Bridge State Disabled - bus on\n");
        setBusPowerAndSetSignal(true);
      }
    } else if (!_timebaseSet && _powerControlEnabled &&
               IORead(&_BusLoadSwitchEnablePin, &enabled)) {
      // If our timebase is not set, not in continuous mode and we've enabled the power manager,
      // we should disable the VBUS
      if (!_powerControlContinuousMode && enabled) {
        bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_INFO, USE_HEADER,
                       "Bridge State Disabled - controller enabled, but timebase is not yet "
                       "set - bus off\n");
        setBusPowerAndSetSignal(false);
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

  if (time_to_sleep_ms > TIMEBASE_NOT_SET_SLEEP_MS && time_to_sleep_ms > MIN_TASK_SLEEP_MS) {
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

/*!
 @brief Determine If Bridge Power Controller Is In Initilization Period

 @see INIT_POWER_ON_TIMEOUT_MS
 @see handleInitState

 @return true if in initialization state,
         false otherwise
 */
bool BridgePowerController::isInInitializationPeriod(void) {
  return (!_initDone || !_timebaseSet) && _powerControlEnabled;
}

/*!
 @brief Calculate The Power Timings During Initialization Period

 @details The initialization period will always have the upcoming off seconds
          set to UNDEFINED as the next sample interval has not yet been
          determined during this state.

 @return Power timings during this moment in the initialization state
 */
BridgePowerController::PowerTimings
BridgePowerController::calculateInitializationTimings(void) {
  static constexpr uint64_t init_uptime_s = 0;
  static constexpr uint32_t init_total_on_s = ms_to_s(INIT_POWER_ON_TIMEOUT_MS);
  uint64_t current_uptime_s = uptimeGetMs() / 1000;

  uint32_t remaining_s = timeRemainingGeneric(init_uptime_s, current_uptime_s, init_total_on_s);

  return PowerTimings{
      .total_on_s = init_total_on_s,
      .remaining_on_s = remaining_s,
      .remaining_off_s = 0,
      .upcoming_off_s = POWER_SERVICE_UNDEFINED,
  };
}

/*!
 @brief Calculate The Power Timings When Bridge Power Controller Is Disabled

 @details The power is on indefinitely during this configuration.

 @return Power timings when bridge power controller is disabled
 */
BridgePowerController::PowerTimings BridgePowerController::calculateDisabledTimings(void) {
  return PowerTimings{
      .total_on_s = POWER_SERVICE_UNDEFINED,
      .remaining_on_s = POWER_SERVICE_UNDEFINED,
      .remaining_off_s = 0,
      .upcoming_off_s = 0,
  };
}

/*!
 @brief Calculate Subsample Timings When Subsampling Is Enabled

 @return Power timings when subsampling is enabled
 */
BridgePowerController::PowerTimings BridgePowerController::calculateSubsampleTimings(void) {
  uint32_t current_time_s = getCurrentTimeS();

  PowerTimings timings = {
      .total_on_s = _sampleDurationS,
      .remaining_on_s = 0,
      .remaining_off_s = 0,
      .upcoming_off_s = _sampleIntervalS - _sampleDurationS,
  };

  uint32_t sample_duration_remain =
      timeRemainingGeneric(_sampleIntervalStartS, current_time_s, _sampleDurationS);

  if (isBridgePowerOn()) {
    timings.remaining_on_s =
        timeRemainingGeneric(_subsampleIntervalStartS, current_time_s, _subsampleDurationS);
  } else {
    timings.remaining_off_s = timeRemainingGeneric(_subsampleIntervalStartS, current_time_s, 0);
  }

  // Account for sample interval off time after sample duration in subsampling
  if (sample_duration_remain >= timings.total_on_s) {
    timings.upcoming_off_s = _subsampleIntervalS - timings.total_on_s;
  } else {
    timings.upcoming_off_s = (_sampleIntervalStartS + _sampleIntervalS) -
                             (_subsampleIntervalStartS + _subsampleDurationS);
  }

  return timings;
}

/*!
 @brief Calculate Power Timings When Enabled And Subsampling Is Disabled

 @return Power timings during normal sample intervals
 */
BridgePowerController::PowerTimings BridgePowerController::calculateSampleTimings(void) {
  uint32_t current_time_s = getCurrentTimeS();

  PowerTimings timings = {
      .total_on_s = _sampleDurationS,
      .remaining_on_s = 0,
      .remaining_off_s = 0,
      .upcoming_off_s = 0,
  };

  if (isBridgePowerOn()) {
    timings.remaining_on_s =
        timeRemainingGeneric(_sampleIntervalStartS, current_time_s, _sampleDurationS);
  } else {
    // _sampleIntervalStartS will be ahead of current time
    timings.remaining_off_s = timeRemainingGeneric(_sampleIntervalStartS, current_time_s, 0);
  }

  timings.upcoming_off_s = _sampleIntervalS - timings.total_on_s;

  return timings;
}

/*!
 @brief Calculate The Power Timings Based Off The Configuration/State Of The Power Controller

 @return Power timings
 */
BridgePowerController::PowerTimings BridgePowerController::calculatePowerTimings(void) {
  if (isInInitializationPeriod()) {
    return calculateInitializationTimings();
  }

  if (!_powerControlEnabled) {
    return calculateDisabledTimings();
  }

  if (_subsamplingEnabled) {
    return calculateSubsampleTimings();
  }

  return calculateSampleTimings();
}

/*!
 @brief Obtain The Power Timings And Create A Power Status Reply

 @details Calculates the power timings and creates a power status reply message
          for the NCP request. This informs the NCP about the current power
          status of the Bristlemouth network

 @param arg BridgePowerController instance

 @return Power status data
 */
bm_serial_power_status_reply_data_t BridgePowerController::getPowerStats(void) {
  bm_serial_power_status_reply_data_t d = {0, 0};

  PowerTimings timings = calculatePowerTimings();

  if (timings.remaining_on_s == POWER_SERVICE_UNDEFINED) {
    d.remaining_on_ms = S_IN_A_DAY;
  } else {
    d.remaining_on_ms = s_to_ms(timings.remaining_on_s);
  }
  d.remaining_off_ms = s_to_ms(timings.remaining_off_s);

  return d;
}

/*!
 @brief Obtain The Power Timings And Create A Power Info Reply

 @details Calculates the power timings and creates a power info reply message
          for the corresponding Bristlemouth service.

 @param arg BridgePowerController instance

 @return Power info data
 */
PowerInfoReplyData BridgePowerController::powerInfoStatsCb(void *arg) {
  PowerInfoReplyData d = {};
  BridgePowerController *power_controller = reinterpret_cast<BridgePowerController *>(arg);

  PowerTimings timings = power_controller->calculatePowerTimings();

  d.total_on_s = timings.total_on_s;
  d.remaining_on_s = timings.remaining_on_s;
  d.upcoming_off_s = timings.upcoming_off_s;

  return d;
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
