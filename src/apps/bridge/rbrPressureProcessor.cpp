#include "rbrPressureProcessor.h"
#include "FreeRTOS.h"
#include "bm_network.h"
#include "bridgeLog.h"
#include "differenceSignal.h"
#include "event_groups.h"
#include "queue.h"
#include "task.h"
#include "task_priorities.h"
#include "timers.h"
#include <inttypes.h>
#include <math.h>
#include <stdlib.h>

#ifndef RAW_PRESSURE_ENABLE
#error "RAW_PRESSURE_ENABLE must be defined"
#endif // RAW_PRESSURE_ENABLE

typedef struct PressureProcessorContext {
  QueueHandle_t q;
  EventGroupHandle_t eg;
  TimerHandle_t sendTimer;
  uint32_t rawSampleS;
  uint32_t diffBitDepth;
  uint32_t maxRawReports;
  double rawDepthThresholdUbar;
  bool started;
} PressureProcessorContext_t;

typedef struct reportMetaData {
  uint32_t magic;
  uint32_t nRawReportsSent;
} reportMetaData_t;

static constexpr EventBits_t kQRcv = 1 << 0;
static constexpr EventBits_t kTimEx = 1 << 1;
static constexpr EventBits_t kAll = kQRcv | kTimEx;
static constexpr uint32_t kRbrSamplePeriodMs = 500;
static constexpr uint32_t kRbrSamplePeriodErrMs = 5;
static constexpr uint32_t kNoInitMagic = 0xcafedaad;
static constexpr char kRbrPressureProcessorTag[] = "[RbrPressureProcessor]";

static void runTask(void *param);
static void diffSigSendTimerCallback(TimerHandle_t xTimer);

static PressureProcessorContext_t _ctx;
#ifndef CI_TEST
static reportMetaData_t _reportMetaData __attribute__((section(".noinit")));
#else  // CI_TEST
static reportMetaData_t _reportMetaData;
#endif // CI_TEST

void rbrPressureProcessorInit(uint32_t rawSampleS, uint32_t diffBitDepth,
                              uint32_t maxRawReports, double rawDepthThresholdUbar) {
  _ctx.q = xQueueCreate(10, sizeof(BmRbrDataMsg::Data));
  configASSERT(_ctx.q);
  _ctx.eg = xEventGroupCreate();
  configASSERT(_ctx.eg);
  _ctx.rawSampleS = rawSampleS;
  _ctx.sendTimer = xTimerCreate("rbrRawSendTimer", pdMS_TO_TICKS(_ctx.rawSampleS * 1000),
                                pdFALSE, NULL, diffSigSendTimerCallback);
  configASSERT(_ctx.sendTimer);
  _ctx.diffBitDepth = diffBitDepth;
  _ctx.maxRawReports = maxRawReports;
  _ctx.rawDepthThresholdUbar = rawDepthThresholdUbar;
  _ctx.started = false;
  if (_reportMetaData.magic != kNoInitMagic) {
    _reportMetaData.magic = kNoInitMagic;
    _reportMetaData.nRawReportsSent = 0;
  }
  configASSERT(xTaskCreate(runTask, "rbrPressureProcessor", configMINIMAL_STACK_SIZE * 4, NULL,
                           RBR_PROCESSOR_TASK_PRIORITY, NULL) == pdPASS);
}

void rbrPressureProcessorStart(void) {
  configASSERT(xTimerStart(_ctx.sendTimer, 10) == pdPASS);
  _ctx.started = true;
}

bool rbrPressureProcessorAddSample(BmRbrDataMsg::Data &rbr_data, uint32_t timeout_ms) {
  if (_ctx.started && xQueueSend(_ctx.q, &rbr_data, pdMS_TO_TICKS(timeout_ms)) == pdTRUE) {
    xEventGroupSetBits(_ctx.eg, kQRcv);
    return true;
  }
  return false;
}

bool rbrPressureProcessorIsStarted(void) { return _ctx.started; }

static void runTask(void *param) {
  (void)param;
  BmRbrDataMsg::Data rbr_data;
  uint32_t num_samples = (_ctx.rawSampleS * 1000) / kRbrSamplePeriodMs;
  size_t diffSignalCapacity = num_samples;
  DifferenceSignal diffSignal(diffSignalCapacity);
  const size_t d_n_size = num_samples * sizeof(double);
  double *d_n = static_cast<double *>(pvPortMalloc(d_n_size));
  while (1) {
    EventBits_t bits = xEventGroupWaitBits(_ctx.eg, kAll, pdTRUE, pdFALSE, portMAX_DELAY);
    if (bits & kTimEx) {
      do {
        if (_reportMetaData.nRawReportsSent >= _ctx.maxRawReports) {
          bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_WARNING, USE_HEADER,
                         "%s nRawReportsSent %" PRIu32 ">= maxRawReports %" PRIu32 "\n",
                         kRbrPressureProcessorTag, _reportMetaData.nRawReportsSent,
                         _ctx.maxRawReports);
          break;
        }
        double signalMean = diffSignal.signalMean();
        if (signalMean < _ctx.rawDepthThresholdUbar) {
          bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_WARNING, USE_HEADER,
                         "%s signalMean %lf < rawDepthThresholdUbar %lf \n",
                         kRbrPressureProcessorTag, signalMean, _ctx.rawDepthThresholdUbar);
          break;
        }
        if (diffSignal.encodeDifferenceSignalToBuffer(d_n, diffSignalCapacity)) {
          bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_INFO, USE_HEADER,
                         "%s Encoded difference signal to buffer \n", kRbrPressureProcessorTag);
          double r0, d0;
          diffSignal.getReferenceSignal(r0);
          // Compute 2nd order difference signal
          DifferenceSignal::differenceSignalFromBuffer(d_n, diffSignalCapacity, d0);
          // TODO - CBOR Encode and send to Spotter
        }
      } while (0);

      diffSignal.clear();
    }
    if (bits & kQRcv) {
      if (xQueueReceive(_ctx.q, &rbr_data, 0) == pdTRUE) {
        if (diffSignal.addSample(rbr_data.pressure_deci_bar)) {
          bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_INFO, USE_HEADER,
                         "%s Added sample %.2f to Diff signal \n", kRbrPressureProcessorTag,
                         rbr_data.pressure_deci_bar);
        } else {
          bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_WARNING, USE_HEADER,
                         "%s Unable to add sample to Diff signal \n", kRbrPressureProcessorTag);
        }
      }
    }
  }
}

static void diffSigSendTimerCallback(TimerHandle_t xTimer) {
  (void)xTimer;
  xEventGroupSetBits(_ctx.eg, kTimEx);
  _ctx.started = false;
}
