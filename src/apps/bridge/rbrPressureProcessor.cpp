#include "rbrPressureProcessor.h"
#include "FreeRTOS.h"
#include "app_pub_sub.h"
#include "bm_rbr_pressure_difference_signal_msg.h"
#include "bm_serial.h"
#include "bridgeLog.h"
#include "device_info.h"
#include "differenceSignal.h"
#include "event_groups.h"
#include "queue.h"
#include "spotter.h"
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
  uint32_t maxRawReports;
  double rawDepthThresholdUbar;
  bool started;
  uint32_t nRawReportsSent;
  uint32_t rbrCodaReadingPeriodMs;
} PressureProcessorContext_t;

static constexpr EventBits_t kQRcv = 1 << 0;
static constexpr EventBits_t kTimEx = 1 << 1;
static constexpr EventBits_t kAll = kQRcv | kTimEx;
static constexpr char kRbrPressureProcessorTag[] = "[RbrPressureProcessor]";
static constexpr size_t cbor_buffer_size = 1024;
static constexpr char kRbrPressureHdrTopic[] = "/sofar/bm_rbr_data";
static constexpr uint32_t kSampleChunkSize = 30;
// 1 decibar is 1/10th of a bar => there are 100,000 millionths of a bar (ubar) in one decibar.
static constexpr double decibar_to_ubar = 100000.0;
static constexpr char kRBRnRawReportsSent[] = "RBRnRawReportsSent";

static void runTask(void *param);
static void diffSigSendTimerCallback(TimerHandle_t xTimer);

static PressureProcessorContext_t _ctx;

void rbrPressureProcessorInit(uint32_t rawSampleS, uint32_t maxRawReports,
                              double rawDepthThresholdUbar, uint32_t rbrCodaReadingPeriodMs) {
  _ctx.rbrCodaReadingPeriodMs = rbrCodaReadingPeriodMs;
  bool success = get_config_uint(BM_CFG_PARTITION_USER, kRBRnRawReportsSent,
                                 strlen(kRBRnRawReportsSent), &_ctx.nRawReportsSent);
  if (!success) {
    _ctx.nRawReportsSent = 0;
  }
  _ctx.q = xQueueCreate(10, sizeof(BmRbrDataMsg::Data));
  configASSERT(_ctx.q);
  _ctx.eg = xEventGroupCreate();
  configASSERT(_ctx.eg);
  _ctx.rawSampleS = rawSampleS;
  _ctx.sendTimer = xTimerCreate("rbrRawSendTimer", pdMS_TO_TICKS(_ctx.rawSampleS * 1000),
                                pdFALSE, NULL, diffSigSendTimerCallback);
  configASSERT(_ctx.sendTimer);
  _ctx.maxRawReports = maxRawReports;
  _ctx.rawDepthThresholdUbar = rawDepthThresholdUbar;
  _ctx.started = false;
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
  const uint32_t diffSignalCapacity = (_ctx.rawSampleS * 1000) / _ctx.rbrCodaReadingPeriodMs;
  DifferenceSignal diffSignal(diffSignalCapacity);
  const size_t d_n_size = diffSignalCapacity * sizeof(double);
  double *d_n = static_cast<double *>(pvPortMalloc(d_n_size));
  uint8_t *cbor_buffer = static_cast<uint8_t *>(pvPortMalloc(cbor_buffer_size));
  while (1) {
    EventBits_t bits = xEventGroupWaitBits(_ctx.eg, kAll, pdTRUE, pdFALSE, portMAX_DELAY);
    if (bits & kTimEx) {
      do {
        if (_ctx.nRawReportsSent >= _ctx.maxRawReports) {
          bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_WARNING, USE_HEADER,
                         "%s nRawReportsSent %" PRIu32 ">= maxRawReports %" PRIu32 "\n",
                         kRbrPressureProcessorTag, _ctx.nRawReportsSent, _ctx.maxRawReports);
          break;
        }
        double signalMean = diffSignal.signalMean();
        double signalMeanUbar = signalMean * decibar_to_ubar;
        if (signalMeanUbar < _ctx.rawDepthThresholdUbar) {
          bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_WARNING, USE_HEADER,
                         "%s signalMean %lf ubar < rawDepthThresholdUbar %lf ubar\n",
                         kRbrPressureProcessorTag, signalMeanUbar, _ctx.rawDepthThresholdUbar);
          break;
        }
        size_t total_samples = diffSignalCapacity;
        if (diffSignal.encodeDifferenceSignalToBuffer(d_n, total_samples)) {
          bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_INFO, USE_HEADER,
                         "%s Encoded difference signal to buffer\n", kRbrPressureProcessorTag);
          double r0, d0;
          diffSignal.getReferenceSignal(r0);
          // Compute 2nd order difference signal
          DifferenceSignal::differenceSignalFromBuffer(d_n, total_samples, d0);
          uint32_t samples_to_send = total_samples;
          static BmRbrPressureDifferenceSignalMsg::Data d;
          uint32_t offset = 0;
          uint32_t sequence_num = 0;
          while (samples_to_send) {
            uint32_t samples_to_send_now =
                samples_to_send > kSampleChunkSize ? kSampleChunkSize : samples_to_send;
            d.header.version = BmRbrPressureDifferenceSignalMsg::VERSION;
            d.header.reading_time_utc_ms = rbr_data.header.reading_time_utc_ms;
            d.header.reading_uptime_millis = rbr_data.header.reading_uptime_millis;
            d.header.sensor_reading_time_ms = rbr_data.header.sensor_reading_time_ms;
            d.total_samples = total_samples;
            d.sequence_num = sequence_num;
            d.num_samples = samples_to_send_now;
            d.residual_0 = r0;
            d.residual_1 = d0;
            d.difference_signal = d_n + offset;
            size_t encoded_len = 0;
            if (BmRbrPressureDifferenceSignalMsg::encode(d, cbor_buffer, cbor_buffer_size,
                                                         &encoded_len) == CborNoError) {
              if (bm_serial_pub(
                      getNodeId(), APP_PUB_SUB_BM_BRIDGE_RBR_HDR_PRESSURE_TOPIC,
                      strlen(APP_PUB_SUB_BM_BRIDGE_RBR_HDR_PRESSURE_TOPIC), cbor_buffer,
                      encoded_len, APP_PUB_SUB_BM_BRIDGE_RBR_HDR_PRESSURE_TYPE,
                      APP_PUB_SUB_BM_BRIDGE_RBR_HDR_PRESSURE_VERSION) == BM_SERIAL_OK) {
                bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_INFO, USE_HEADER,
                               "%s Sent difference signal to Spotter\n",
                               kRbrPressureProcessorTag);
                offset += samples_to_send_now;
                samples_to_send -= samples_to_send_now;
                sequence_num++;
              } else {
                bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_WARNING, USE_HEADER,
                               "%s Failed to publish difference signal to Spotter\n",
                               kRbrPressureProcessorTag);
                break;
              }
            } else {
              bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_WARNING, USE_HEADER,
                             "%s Failed to encode difference signal\n",
                             kRbrPressureProcessorTag);
            }
            vTaskDelay(100);
          }
          _ctx.nRawReportsSent++;
          bool success = set_config_uint(BM_CFG_PARTITION_USER, kRBRnRawReportsSent,
                                         strlen(kRBRnRawReportsSent), _ctx.nRawReportsSent);
          if (success) {
            save_config(BM_CFG_PARTITION_USER, false);
          }
        }
      } while (0);

      diffSignal.clear();
    }
    if (bits & kQRcv) {
      if (xQueueReceive(_ctx.q, &rbr_data, 0) == pdTRUE) {
        if (_ctx.started) {
          if (diffSignal.addSample(rbr_data.pressure_deci_bar)) {
            bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_INFO, USE_HEADER,
                           "%s Added reading %.4f to Diff signal\n", kRbrPressureProcessorTag,
                           rbr_data.pressure_deci_bar);
          } else {
            bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_WARNING, USE_HEADER,
                           "%s Unable to add reading to Diff signal, buffer is full\n",
                           kRbrPressureProcessorTag);
          }
        } else {
          bridgeLogPrint(
              BRIDGE_SYS, BM_COMMON_LOG_LEVEL_WARNING, USE_HEADER,
              "%s Not adding late reading %.4f to Diff signal because timer has stopped\n",
              kRbrPressureProcessorTag, rbr_data.pressure_deci_bar);
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
