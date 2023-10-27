#pragma once

#include "configuration.h"

namespace AppConfig {

constexpr const char *SAMPLE_INTERVAL_MS = "sampleIntervalMs";
constexpr const char *SAMPLE_DURATION_MS = "sampleDurationMs";
constexpr const char *SUBSAMPLE_INTERVAL_MS = "subsampleIntervalMs";
constexpr const char *SUBSAMPLE_DURATION_MS = "subsampleDurationMs";
constexpr const char *SUBSAMPLE_ENABLED = "subsampleEnabled";
constexpr const char *BRIDGE_POWER_CONTROLLER_ENABLED = "bridgePowerControllerEnabled";
constexpr const char *ALIGNMENT_INTERVAL_5MIN = "alignmentInterval5Min";
constexpr const char *TRANSMIT_AGGREGATIONS = "transmitAggregations";
constexpr const char *SAMPLES_PER_REPORT = "samplesPerReport";

} // namespace AppConfig

struct power_config_s {
  uint32_t sampleIntervalMs, sampleDurationMs, subsampleIntervalMs, subsampleDurationMs,
      subsampleEnabled, bridgePowerControllerEnabled, alignmentInterval5Min;
};

power_config_s getPowerConfigs(cfg::Configuration &syscfg);
