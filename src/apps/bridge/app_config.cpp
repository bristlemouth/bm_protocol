#include "app_config.h"
#include "bridgePowerController.h"

power_config getPowerConfigs(cfg::Configuration &syscfg) {
  power_config pwrcfg;

  pwrcfg.sampleIntervalMs = BridgePowerController::DEFAULT_SAMPLE_INTERVAL_S * 1000;
  syscfg.getConfig(AppConfig::SAMPLE_DURATION_MS, strlen(AppConfig::SAMPLE_DURATION_MS),
                   pwrcfg.sampleIntervalMs);

  pwrcfg.sampleDurationMs = BridgePowerController::DEFAULT_SAMPLE_DURATION_S * 1000;
  syscfg.getConfig(AppConfig::SAMPLE_DURATION_MS, strlen(AppConfig::SAMPLE_DURATION_MS),
                   pwrcfg.sampleDurationMs);

  pwrcfg.subSampleIntervalMs = BridgePowerController::DEFAULT_SUBSAMPLE_INTERVAL_S * 1000;
  syscfg.getConfig(AppConfig::SUBSAMPLE_INTERVAL_MS, strlen(AppConfig::SUBSAMPLE_INTERVAL_MS),
                   pwrcfg.subSampleIntervalMs);

  pwrcfg.subsampleDurationMs = BridgePowerController::DEFAULT_SUBSAMPLE_DURATION_S * 1000;
  syscfg.getConfig(AppConfig::SUBSAMPLE_DURATION_MS, strlen(AppConfig::SUBSAMPLE_DURATION_MS),
                   pwrcfg.subsampleDurationMs);

  pwrcfg.subsampleEnabled = BridgePowerController::DEFAULT_SUBSAMPLE_ENABLED;
  syscfg.getConfig(AppConfig::SUBSAMPLE_ENABLED, strlen(AppConfig::SUBSAMPLE_ENABLED),
                   pwrcfg.subsampleEnabled);

  pwrcfg.bridgePowerControllerEnabled = BridgePowerController::DEFAULT_POWER_CONTROLLER_ENABLED;
  syscfg.getConfig(AppConfig::BRIDGE_POWER_CONTROLLER_ENABLED,
                   strlen(AppConfig::BRIDGE_POWER_CONTROLLER_ENABLED),
                   pwrcfg.bridgePowerControllerEnabled);

  pwrcfg.alignmentInterval5Min = BridgePowerController::DEFAULT_ALIGNMENT_5_MIN_INTERVAL;
  syscfg.getConfig(AppConfig::ALIGNMENT_INTERVAL_5MIN,
                   strlen(AppConfig::ALIGNMENT_INTERVAL_5MIN), pwrcfg.alignmentInterval5Min);

  return pwrcfg;
}
