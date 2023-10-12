#pragma once

#include "FreeRTOS.h"
#include "event_groups.h"
#include "io.h"
#include "task.h"

#include "eth_adin2111.h"
#include <stdint.h>

class BridgePowerController {
public:
  explicit BridgePowerController(
      IOPinHandle_t &BusPowerPin,
      uint32_t sampleIntervalMs = DEFAULT_SAMPLE_INTERVAL_S * 1000,
      uint32_t sampleDurationMs = DEFAULT_SAMPLE_DURATION_S * 1000,
      uint32_t subsampleIntervalMs = DEFAULT_SUBSAMPLE_INTERVAL_S * 1000,
      uint32_t subsampleDurationMs = DEFAULT_SUBSAMPLE_DURATION_S * 1000,
      bool subSamplingEnabled = false, bool powerControllerEnabled = false,
      uint32_t alignmentS = DEFAULT_ALIGNMENT_S);
  void powerControlEnable(bool enable);
  bool isPowerControlEnabled();
  void subSampleEnable(bool enable);
  bool isSubsampleEnabled();
  bool waitForSignal(bool on, TickType_t ticks_to_wait);
  bool isBridgePowerOn(void);
  bool initPeriodElapsed(void);

  // Shim function for FreeRTOS compatibility, should not be called as part of the public API.
  void _update(void); // PRIVATE
private:
  void powerBusAndSetSignal(bool on, bool notifyL2 = true);
  static void powerControllerRun(void *arg);
  bool getAdinDevice();
  void checkAndUpdateRTC();
  uint32_t getEpochS();
  uint32_t alignEpoch(uint32_t epochS);

public:
  static constexpr uint32_t OFF = (1 << 0);
  static constexpr uint32_t ON = (1 << 1);
  static constexpr uint32_t DEFAULT_POWER_CONTROLLER_ENABLED = 0;
  static constexpr uint32_t DEFAULT_SAMPLE_INTERVAL_S = (20 * 60);
  static constexpr uint32_t DEFAULT_SAMPLE_DURATION_S = (5 * 60);
  static constexpr uint32_t DEFAULT_SUBSAMPLE_ENABLED = 0;
  static constexpr uint32_t DEFAULT_SUBSAMPLE_INTERVAL_S = (60);
  static constexpr uint32_t DEFAULT_SUBSAMPLE_DURATION_S = (30);
  static constexpr uint32_t MIN_SAMPLE_DURATION_S = (1);
  static constexpr uint32_t MIN_SAMPLE_INTERVAL_S = (1);
  static constexpr uint32_t MAX_SAMPLE_INTERVAL_S = (24 * 60 * 60);
  static constexpr uint32_t MAX_SAMPLE_DURATION_S = (24 * 60 * 60);
  static constexpr uint32_t MIN_SUBSAMPLE_INTERVAL_S = (1);
  static constexpr uint32_t MIN_SUBSAMPLE_DURATION_S = (1);
  static constexpr uint32_t MAX_SUBSAMPLE_INTERVAL_S = (60 * 60);
  static constexpr uint32_t MAX_SUBSAMPLE_DURATION_S = (60 * 60);
  static constexpr uint32_t DEFAULT_ALIGNMENT_S = (5 * 60);
  static constexpr uint32_t MAX_ALIGNMENT_S = (24 * 60 * 60);
  static constexpr uint32_t DEFAULT_ALIGNMENT_5_MIN_INTERVAL = (1);

private:
  static constexpr uint32_t MIN_TASK_SLEEP_MS = (1000);
  static constexpr uint32_t INIT_POWER_ON_TIMEOUT_MS = (2 * 60 * 1000);

private:
  IOPinHandle_t &_BusPowerPin;
  bool _powerControlEnabled;
  uint32_t _sampleIntervalS;
  uint32_t _sampleDurationS;
  uint32_t _subsampleIntervalS;
  uint32_t _subsampleDurationS;
  uint32_t _sampleIntervalStartS;
  uint32_t _subSampleIntervalStartS;
  uint32_t _alignmentS;

  bool _rtcSet;
  bool _initDone;
  bool _subSamplingEnabled;
  adin2111_DeviceHandle_t _adin_handle;
  EventGroupHandle_t _busPowerEventGroup;
  TaskHandle_t _task_handle;
};
