#include "app_config.h"
#include "bridgeLog.h"
#include "bridgePowerController.h"
#include "rbrCodaSensor.h"
#ifdef RAW_PRESSURE_ENABLE
#include "rbrPressureProcessor.h"
#endif // RAW_PRESSURE_ENABLE

power_config_s getPowerConfigs(void) {
  power_config_s pwrcfg;

  bool save = false;
  pwrcfg.sampleIntervalMs = BridgePowerController::DEFAULT_SAMPLE_INTERVAL_S * 1000;
  if (!get_config_uint(BM_CFG_PARTITION_SYSTEM, AppConfig::SAMPLE_INTERVAL_MS,
                       strlen(AppConfig::SAMPLE_INTERVAL_MS), &pwrcfg.sampleIntervalMs)) {
    bridgeLogPrint(BRIDGE_CFG, BM_COMMON_LOG_LEVEL_INFO, USE_HEADER,
                   "Failed to get sample interval from config, using default value and writing "
                   "to config: %" PRIu32 "ms\n",
                   pwrcfg.sampleIntervalMs);
    set_config_uint(BM_CFG_PARTITION_SYSTEM, AppConfig::SAMPLE_INTERVAL_MS,
                    strlen(AppConfig::SAMPLE_INTERVAL_MS), pwrcfg.sampleIntervalMs);
    save = true;
  }

  pwrcfg.sampleDurationMs = BridgePowerController::DEFAULT_SAMPLE_DURATION_S * 1000;
  if (!get_config_uint(BM_CFG_PARTITION_SYSTEM, AppConfig::SAMPLE_DURATION_MS,
                       strlen(AppConfig::SAMPLE_DURATION_MS), &pwrcfg.sampleDurationMs)) {
    bridgeLogPrint(BRIDGE_CFG, BM_COMMON_LOG_LEVEL_INFO, USE_HEADER,
                   "Failed to get sample duration from config, using default value and writing "
                   "to config: %" PRIu32 "ms\n",
                   pwrcfg.sampleDurationMs);
    set_config_uint(BM_CFG_PARTITION_SYSTEM, AppConfig::SAMPLE_DURATION_MS,
                    strlen(AppConfig::SAMPLE_DURATION_MS), pwrcfg.sampleDurationMs);
    save = true;
  }

  pwrcfg.subsampleIntervalMs = BridgePowerController::DEFAULT_SUBSAMPLE_INTERVAL_S * 1000;
  if (!get_config_uint(BM_CFG_PARTITION_SYSTEM, AppConfig::SUBSAMPLE_INTERVAL_MS,
                       strlen(AppConfig::SUBSAMPLE_INTERVAL_MS), &pwrcfg.subsampleIntervalMs)) {
    bridgeLogPrint(BRIDGE_CFG, BM_COMMON_LOG_LEVEL_INFO, USE_HEADER,
                   "Failed to get subsample interval from config, using default value and "
                   "writing to config: %" PRIu32 "ms\n",
                   pwrcfg.subsampleIntervalMs);
    set_config_uint(BM_CFG_PARTITION_SYSTEM, AppConfig::SUBSAMPLE_INTERVAL_MS,
                    strlen(AppConfig::SUBSAMPLE_INTERVAL_MS), pwrcfg.subsampleIntervalMs);
    save = true;
  }

  pwrcfg.subsampleDurationMs = BridgePowerController::DEFAULT_SUBSAMPLE_DURATION_S * 1000;
  if (!get_config_uint(BM_CFG_PARTITION_SYSTEM, AppConfig::SUBSAMPLE_DURATION_MS,
                       strlen(AppConfig::SUBSAMPLE_DURATION_MS), &pwrcfg.subsampleDurationMs)) {
    bridgeLogPrint(BRIDGE_CFG, BM_COMMON_LOG_LEVEL_INFO, USE_HEADER,
                   "Failed to get subsample duration from config, using default value and "
                   "writing to config: %" PRIu32 "ms\n",
                   pwrcfg.subsampleDurationMs);
    set_config_uint(BM_CFG_PARTITION_SYSTEM, AppConfig::SUBSAMPLE_DURATION_MS,
                    strlen(AppConfig::SUBSAMPLE_DURATION_MS), pwrcfg.subsampleDurationMs);
    save = true;
  }

  pwrcfg.subsampleEnabled = BridgePowerController::DEFAULT_SUBSAMPLE_ENABLED;
  if (!get_config_uint(BM_CFG_PARTITION_SYSTEM, AppConfig::SUBSAMPLE_ENABLED,
                       strlen(AppConfig::SUBSAMPLE_ENABLED), &pwrcfg.subsampleEnabled)) {
    bridgeLogPrint(BRIDGE_CFG, BM_COMMON_LOG_LEVEL_INFO, USE_HEADER,
                   "Failed to get subsample enabled from config, using default value and "
                   "writing to config: %" PRIu32 "\n",
                   pwrcfg.subsampleEnabled);
    set_config_uint(BM_CFG_PARTITION_SYSTEM, AppConfig::SUBSAMPLE_ENABLED,
                    strlen(AppConfig::SUBSAMPLE_ENABLED), pwrcfg.subsampleEnabled);
    save = true;
  }

  pwrcfg.bridgePowerControllerEnabled = BridgePowerController::DEFAULT_POWER_CONTROLLER_ENABLED;
  if (!get_config_uint(BM_CFG_PARTITION_SYSTEM, AppConfig::BRIDGE_POWER_CONTROLLER_ENABLED,
                       strlen(AppConfig::BRIDGE_POWER_CONTROLLER_ENABLED),
                       &pwrcfg.bridgePowerControllerEnabled)) {
    bridgeLogPrint(BRIDGE_CFG, BM_COMMON_LOG_LEVEL_INFO, USE_HEADER,
                   "Failed to get bridge power controller enabled from config, using default "
                   "value and writing to config: %" PRIu32 "\n",
                   pwrcfg.bridgePowerControllerEnabled);
    set_config_uint(BM_CFG_PARTITION_SYSTEM, AppConfig::BRIDGE_POWER_CONTROLLER_ENABLED,
                    strlen(AppConfig::BRIDGE_POWER_CONTROLLER_ENABLED),
                    pwrcfg.bridgePowerControllerEnabled);
    save = true;
  }

  pwrcfg.alignmentInterval5Min = BridgePowerController::DEFAULT_ALIGNMENT_5_MIN_INTERVAL;
  if (!get_config_uint(BM_CFG_PARTITION_SYSTEM, AppConfig::ALIGNMENT_INTERVAL_5MIN,
                       strlen(AppConfig::ALIGNMENT_INTERVAL_5MIN),
                       &pwrcfg.alignmentInterval5Min)) {
    bridgeLogPrint(BRIDGE_CFG, BM_COMMON_LOG_LEVEL_INFO, USE_HEADER,
                   "Failed to get alignment interval from config, using default value and "
                   "writing to config: %" PRIu32 "ms\n",
                   pwrcfg.alignmentInterval5Min);
    set_config_uint(BM_CFG_PARTITION_SYSTEM, AppConfig::ALIGNMENT_INTERVAL_5MIN,
                    strlen(AppConfig::ALIGNMENT_INTERVAL_5MIN), pwrcfg.alignmentInterval5Min);
    save = true;
  }

  pwrcfg.ticksSamplingEnabled = BridgePowerController::DEFAULT_TICKS_SAMPLING_ENABLED;
  if (!get_config_uint(BM_CFG_PARTITION_SYSTEM, AppConfig::TICKS_SAMPLING_ENABLED,
                       strlen(AppConfig::TICKS_SAMPLING_ENABLED),
                       &pwrcfg.ticksSamplingEnabled)) {
    bridgeLogPrint(BRIDGE_CFG, BM_COMMON_LOG_LEVEL_INFO, USE_HEADER,
                   "Failed to get ticks sampling enabled from config, using default value and "
                   "writing to config: %" PRIu32 "ms\n",
                   pwrcfg.ticksSamplingEnabled);
    set_config_uint(BM_CFG_PARTITION_SYSTEM, AppConfig::TICKS_SAMPLING_ENABLED,
                    strlen(AppConfig::TICKS_SAMPLING_ENABLED), pwrcfg.ticksSamplingEnabled);
    save = true;
  }
  if (save) {
    save_config(BM_CFG_PARTITION_SYSTEM, false);
  }

  return pwrcfg;
}

#ifdef RAW_PRESSURE_ENABLE
raw_pressure_config_s getRawPressureConfigs(void) {
  raw_pressure_config_s cfg;

  bool save = false;
  cfg.rawSampleS = RbrPressureProcessor::DEFAULT_RAW_SAMPLE_S;
  if (!get_config_uint(BM_CFG_PARTITION_SYSTEM, AppConfig::RBR_RAW_DIFFERENTIAL_SIGNAL_PERIOD_S,
                       strlen(AppConfig::RBR_RAW_DIFFERENTIAL_SIGNAL_PERIOD_S),
                       &cfg.rawSampleS)) {
    bridgeLogPrint(BRIDGE_CFG, BM_COMMON_LOG_LEVEL_INFO, USE_HEADER,
                   "Failed to get rbr pressure differential sample period from config, using "
                   "default value and writing "
                   "to config: %" PRIu32 "ms\n",
                   cfg.rawSampleS);
    set_config_uint(BM_CFG_PARTITION_SYSTEM, AppConfig::RBR_RAW_DIFFERENTIAL_SIGNAL_PERIOD_S,
                    strlen(AppConfig::RBR_RAW_DIFFERENTIAL_SIGNAL_PERIOD_S), cfg.rawSampleS);
    save = true;
  }
  cfg.maxRawReports = RbrPressureProcessor::DEFAULT_MAX_RAW_REPORTS;
  if (!get_config_uint(BM_CFG_PARTITION_SYSTEM, AppConfig::RBR_MAX_RAW_REPORTS,
                       strlen(AppConfig::RBR_MAX_RAW_REPORTS), &cfg.maxRawReports)) {
    bridgeLogPrint(BRIDGE_CFG, BM_COMMON_LOG_LEVEL_INFO, USE_HEADER,
                   "Failed to get rbr pressure differential max raw reports from config, using "
                   "default value and writing "
                   "to config: %" PRIu32 "ms\n",
                   cfg.maxRawReports);
    set_config_uint(BM_CFG_PARTITION_SYSTEM, AppConfig::RBR_MAX_RAW_REPORTS,
                    strlen(AppConfig::RBR_MAX_RAW_REPORTS), cfg.maxRawReports);
    save = true;
  }
  cfg.rawDepthThresholdUbar = RbrPressureProcessor::DEFAULT_RAW_DEPTH_THRESHOLD_UBAR;
  if (!get_config_float(BM_CFG_PARTITION_SYSTEM, AppConfig::RBR_RAW_DEPTH_THRESHOLD_UBAR,
                        strlen(AppConfig::RBR_RAW_DEPTH_THRESHOLD_UBAR),
                        &cfg.rawDepthThresholdUbar)) {
    bridgeLogPrint(BRIDGE_CFG, BM_COMMON_LOG_LEVEL_INFO, USE_HEADER,
                   "Failed to get rbr pressure differential threshold from config, using "
                   "default value and writing "
                   "to config: %" PRIu32 "ms\n",
                   cfg.rawDepthThresholdUbar);
    set_config_uint(BM_CFG_PARTITION_SYSTEM, AppConfig::RBR_RAW_DEPTH_THRESHOLD_UBAR,
                    strlen(AppConfig::RBR_RAW_DEPTH_THRESHOLD_UBAR), cfg.rawDepthThresholdUbar);
    save = true;
  }
  cfg.rbrCodaReadingPeriodMs = RbrCodaSensor::DEFAULT_RBR_CODA_READING_PERIOD_MS;
  if (!get_config_uint(BM_CFG_PARTITION_SYSTEM, AppConfig::RBR_CODA_READING_PERIOD_MS,
                       strlen(AppConfig::RBR_CODA_READING_PERIOD_MS),
                       &cfg.rbrCodaReadingPeriodMs)) {
    bridgeLogPrint(BRIDGE_CFG, BM_COMMON_LOG_LEVEL_INFO, USE_HEADER,
                   "Failed to get rbr coda reading period from config, using default value and "
                   "writing to config: %" PRIu32 "ms\n",
                   cfg.rbrCodaReadingPeriodMs);
    set_config_uint(BM_CFG_PARTITION_SYSTEM, AppConfig::RBR_CODA_READING_PERIOD_MS,
                    strlen(AppConfig::RBR_CODA_READING_PERIOD_MS), cfg.rbrCodaReadingPeriodMs);
    save = true;
  }
  if (save) {
    save_config(BM_CFG_PARTITION_SYSTEM, false);
  }

  return cfg;
}
#endif // RAW_PRESSURE_ENABLE
