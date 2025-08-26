#pragma once

#include "FreeRTOS.h"
#include "event_groups.h"
#include "io.h"
#include "power_info_service.h"
#include "task.h"
#include <stdint.h>

class BridgePowerController {
public:
  struct Config {
    IOPinHandle_t &BusPowerPin;
    IOPinHandle_t &BoostPowerPin;
    uint32_t sampleIntervalMs = DEFAULT_SAMPLE_INTERVAL_S * 1000;
    uint32_t sampleDurationMs = DEFAULT_SAMPLE_DURATION_S * 1000;
    uint32_t subsampleIntervalMs = DEFAULT_SUBSAMPLE_INTERVAL_S * 1000;
    uint32_t subsampleDurationMs = DEFAULT_SUBSAMPLE_DURATION_S * 1000;
    bool subsamplingEnabled = static_cast<bool>(DEFAULT_SUBSAMPLE_ENABLED);
    bool powerControllerEnabled = static_cast<bool>(DEFAULT_POWER_CONTROLLER_ENABLED);
    uint32_t alignmentS = DEFAULT_ALIGNMENT_S;
    bool ticksSamplingEnabled = static_cast<bool>(DEFAULT_TICKS_SAMPLING_ENABLED);
  };

  explicit BridgePowerController(const Config &config);
  void powerControlEnable(bool enable);
  bool isPowerControlEnabled();
  void subsampleEnable(bool enable);
  bool isSubsampleEnabled();
  bool waitForSignal(bool on, TickType_t ticks_to_wait);
  bool isBridgePowerOn(void);
  bool initPeriodElapsed(void);
  void validateConfig(void);

  // Shim function for FreeRTOS compatibility, should not be called as part of the public API.
  void _update(void); // PRIVATE
  // Public member only for testability. Not part of the public API.
  uint32_t _alignNextInterval(uint32_t nowEpochS, uint32_t lastIntervalStartS,
                              uint32_t sampleIntervalS);

private:
  void powerBusAndSetSignal(bool on, bool notifyL2 = true);
  static void powerControllerRun(void *arg);
  static PowerInfoReplyData powerInfoStatsCb(void *arg);
  void checkAndUpdateTimebase();
  uint32_t getCurrentTimeS();
  void stateLogPrintTarget(const char *state, uint32_t target);

public:
  static constexpr uint32_t OFF = (1 << 0);
  static constexpr uint32_t ON = (1 << 1);
  static constexpr uint32_t DEFAULT_POWER_CONTROLLER_ENABLED = 1;
  static constexpr uint32_t DEFAULT_SAMPLE_INTERVAL_S = (30 * 60);
  static constexpr uint32_t DEFAULT_SAMPLE_DURATION_S = ((5 * 60) + 10);
  static constexpr uint32_t DEFAULT_SUBSAMPLE_ENABLED = 0;
  static constexpr uint32_t DEFAULT_SUBSAMPLE_INTERVAL_S = (60);
  static constexpr uint32_t DEFAULT_SUBSAMPLE_DURATION_S = (30);
  static constexpr uint32_t MIN_SAMPLE_DURATION_S = (6);
  static constexpr uint32_t MIN_SAMPLE_INTERVAL_S =
      (6); // Set to 6s to allow for enough time for devices to broadcast for the topology.
  static constexpr uint32_t MAX_SAMPLE_INTERVAL_S = (24 * 60 * 60);
  static constexpr uint32_t MAX_SAMPLE_DURATION_S = (24 * 60 * 60);
  static constexpr uint32_t MIN_SUBSAMPLE_INTERVAL_S = (6);
  static constexpr uint32_t MIN_SUBSAMPLE_DURATION_S = (6);
  static constexpr uint32_t MAX_SUBSAMPLE_INTERVAL_S = (60 * 60);
  static constexpr uint32_t MAX_SUBSAMPLE_DURATION_S = (60 * 60);
  static constexpr uint32_t DEFAULT_ALIGNMENT_S = (5 * 60);
  static constexpr uint32_t ALIGNMENT_INCREMENT_S = (5 * 60);
  static constexpr uint32_t MAX_ALIGNMENT_S = (24 * 60 * 60);
  static constexpr uint32_t DEFAULT_ALIGNMENT_5_MIN_INTERVAL = (1);
  static constexpr uint32_t DEFAULT_TICKS_SAMPLING_ENABLED = (0);
  static constexpr uint32_t CAPACITOR_CHARGE_DELAY_MS = 15;
  static constexpr uint32_t MIN_TASK_SLEEP_MS = (1000);
  static constexpr uint32_t INIT_POWER_ON_TIMEOUT_MS = (2 * 60 * 1000);

private:
  IOPinHandle_t &_BusPowerPin;
  IOPinHandle_t &_BoostPowerPin;
  bool _powerControlEnabled;
  uint32_t _sampleIntervalS;
  uint32_t _sampleDurationS;
  uint32_t _subsampleIntervalS;
  uint32_t _subsampleDurationS;
  uint32_t _sampleIntervalStartS;
  uint32_t _subsampleIntervalStartS;
  uint32_t _alignmentS;
  bool _ticksSamplingEnabled;

  bool _timebaseSet;
  bool _initDone;
  bool _subsamplingEnabled;
  bool _configError;
  EventGroupHandle_t _busPowerEventGroup;
  TaskHandle_t _task_handle;
};
