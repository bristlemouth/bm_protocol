#pragma once

#include "bm_rbr_data_msg.h"
#include "configuration.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

namespace RbrPressureProcessor {
constexpr uint32_t DEFAULT_RAW_SAMPLE_S = 275;
constexpr uint32_t DEFAULT_MAX_RAW_REPORTS = 200;
constexpr double DEFAULT_RAW_DEPTH_THRESHOLD_UBAR = 1500000.0;
} // namespace RbrPressureProcessor

void rbrPressureProcessorInit(uint32_t rawSampleS, uint32_t maxRawReports,
                              double rawDepthThresholdUbar, uint32_t rbrCodaReadingPeriodMs);

bool rbrPressureProcessorAddSample(BmRbrDataMsg::Data &rbr_data, uint32_t timeout_ms);

void rbrPressureProcessorStart(void);

bool rbrPressureProcessorIsStarted(void);

#ifdef __cplusplus
}
#endif // __cplusplus