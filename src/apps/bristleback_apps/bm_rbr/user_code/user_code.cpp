#include "user_code.h"
#include "FreeRTOS.h"
#include "bm_printf.h"
#include "bm_pubsub.h"
#include "bm_rbr_data_msg.h"
#include "bsp.h"
#include "debug.h"
#include "device_info.h"
#include "rbr_sensor.h"
#include "memfault_platform_core.h"
#include "reset_reason.h"
#include "sensorWatchdog.h"
#include "uptime.h"
#include "util.h"

traceEvent_t user_traceEvents[TRACE_BUFF_LEN];
uint32_t user_expectedTicks = 0;
uint32_t user_fullTicksLeft = 0;
uint32_t user_lpuart_start_cpu_cycles = 0;
uint32_t user_lpuart_stop_cpu_cycles = 0;

static constexpr char BM_RBR_WATCHDOG_ID[] = "bm_rbr";
static constexpr uint32_t PAYLOAD_WATCHDOG_TIMEOUT_MS = 10 * 1000;
// Note that PROBE_TIME_PERIOD_MS should be different than the watchdog timeout
// to avoid always trying to probe while the sensor is powered down.
static constexpr uint32_t PROBE_TIME_PERIOD_MS = 8 * 1000;
static constexpr uint32_t BM_RBR_DATA_MSG_MAX_SIZE = 256;
static constexpr uint8_t NO_MAX_TRIGGER = 0;

extern cfg::Configuration *systemConfigurationPartition;
static RbrSensor rbr_sensor;
static char bmRbrTopic[BM_TOPIC_MAX_LEN];
static int bmRbrTopicStrLen;

// Configurable VOUT off duration when we want to reboot the RBR sensor.
// We do this to recover from FTL: Failure To Launch.
static constexpr char CFG_FTL_RECOVERY_MS[] = "ftlRecoveryMs";
static uint32_t ftl_recovery_ms = 800;
static uint64_t last_payload_power_on_time = 0;

static bool BmRbrWatchdogHandler(void *arg);
static int createBmRbrDataTopic(void);

static bool sent_reset_reason = false;
static constexpr char TRACE_BUFF_ENABLE[] = "traceBuffEnable";
static uint32_t traceBuffEnable = 0;

void setup(void) {
  configASSERT(systemConfigurationPartition);
  uint32_t sensor_type = static_cast<uint32_t>(BmRbrDataMsg::SensorType_t::UNKNOWN);
  systemConfigurationPartition->getConfig(RbrSensor::CFG_RBR_TYPE,
                                          strlen(RbrSensor::CFG_RBR_TYPE), sensor_type);
  systemConfigurationPartition->getConfig(CFG_FTL_RECOVERY_MS, strlen(CFG_FTL_RECOVERY_MS),
                                          ftl_recovery_ms);
  rbr_sensor.init(static_cast<BmRbrDataMsg::SensorType_t>(sensor_type), PROBE_TIME_PERIOD_MS);
  SensorWatchdog::SensorWatchdogAdd(BM_RBR_WATCHDOG_ID, PAYLOAD_WATCHDOG_TIMEOUT_MS,
                                    BmRbrWatchdogHandler, NO_MAX_TRIGGER,
                                    RbrSensor::RBR_RAW_LOG);
  systemConfigurationPartition->getConfig(TRACE_BUFF_ENABLE, strlen(TRACE_BUFF_ENABLE), traceBuffEnable);
  bmRbrTopicStrLen = createBmRbrDataTopic();
  IOWrite(&BB_VBUS_EN, 0);
  vTaskDelay(pdMS_TO_TICKS(100)); // Wait for Vbus to stabilize
  IOWrite(&BB_PL_BUCK_EN, 0);
  last_payload_power_on_time = uptimeGetMs();
}

void loop(void) {
  // Read and handle line from sensor
  // which may be data or a command response
  static BmRbrDataMsg::Data d;

  if (uptimeGetMs() > 5000 && !sent_reset_reason) {
    sent_reset_reason = true;
    ResetReason_t resetReason = checkResetReason();
    uint32_t pc = memfault_get_pc();
    uint32_t lr = memfault_get_lr();
    bm_printf(0, "Reset Reason: %d: %s, PC: 0x%" PRIx32 ", LR: 0x%" PRIx32 "\n", resetReason, getResetReasonString(), pc, lr);
    bm_fprintf(0, "reset.log", "Reset Reason: %d: %s, PC: 0x%" PRIx32 ", LR: 0x%" PRIx32 "\n", resetReason, getResetReasonString(), pc, lr);
    printf("Reset Reason: %d: %s, PC: 0x%" PRIx32 ", LR: 0x%" PRIx32 "\n", resetReason, getResetReasonString(), pc, lr);
    bm_fprintf(0, "reset.log", "Full ticks left: %" PRIu32 ", expected ticks: %" PRIu32 "\n", user_fullTicksLeft, user_expectedTicks);
    bm_printf(0, "Full ticks left: %" PRIu32 ", expected ticks: %" PRIu32 "", user_fullTicksLeft, user_expectedTicks);
    bm_fprintf(0, "reset.log", "Last lpuart start: %" PRIu32 ", stop: %" PRIu32 " = %" PRIu32 "ms*1000\n", user_lpuart_start_cpu_cycles, user_lpuart_stop_cpu_cycles, ((user_lpuart_stop_cpu_cycles - user_lpuart_start_cpu_cycles)/160));
    bm_printf(0, "Last lpuart start: %" PRIu32 ", stop: %" PRIu32 " = %" PRIu32 "ms*1000", user_lpuart_start_cpu_cycles, user_lpuart_stop_cpu_cycles, ((user_lpuart_stop_cpu_cycles - user_lpuart_start_cpu_cycles)/160));
    if (traceBuffEnable) {
      bm_printf(0, "Trace buffer:");
      bm_fprintf(0, "trace.log", "Trace buffer:\n");
      for (uint16_t i = 0; i < TRACE_BUFF_LEN; i++) {
        vTaskDelay(100);
        bm_printf(0, "timestamp: %" PRIu32 ", Event type: %d, Arg: 0x%" PRIx32 "", user_traceEvents[i].timestamp, user_traceEvents[i].eventType, user_traceEvents[i].arg);
        bm_fprintf(0, "trace.log", "timestamp: %" PRIu32 ", Event type: %d, Arg: 0x%" PRIx32 "\n", user_traceEvents[i].timestamp, user_traceEvents[i].eventType, user_traceEvents[i].arg);
      }
      bm_printf(0, "End of trace buffer");
    }
  }


  if (rbr_sensor.getData(d)) {
    SensorWatchdog::SensorWatchdogPet(BM_RBR_WATCHDOG_ID);
    static uint8_t cbor_buf[BM_RBR_DATA_MSG_MAX_SIZE];
    memset(cbor_buf, 0, sizeof(cbor_buf));
    size_t encoded_len = 0;
    if (BmRbrDataMsg::encode(d, cbor_buf, sizeof(cbor_buf), &encoded_len) == CborNoError) {
      bm_pub_wl(bmRbrTopic, bmRbrTopicStrLen, cbor_buf, encoded_len, 0);
    } else {
      printf("Failed to encode data message\n");
    }
  }

  // Probe sensor type periodically, write only, no read
  rbr_sensor.maybeProbeType(last_payload_power_on_time);
}

static bool BmRbrWatchdogHandler(void *arg) {
  (void)arg;
  bm_fprintf(0, RbrSensor::RBR_RAW_LOG, "DEBUG - attempting FTL recovery\n");
  bm_printf(0, "DEBUG - attempting FTL recovery");
  printf("DEBUG - attempting FTL recovery\n");
  IOWrite(&BB_PL_BUCK_EN, 1);
  vTaskDelay(pdMS_TO_TICKS(ftl_recovery_ms));
  rbr_sensor.flush();
  IOWrite(&BB_PL_BUCK_EN, 0);
  last_payload_power_on_time = uptimeGetMs();
  return true;
}

static int createBmRbrDataTopic(void) {
  int topiclen = snprintf(bmRbrTopic, BM_TOPIC_MAX_LEN,
                          "sensor/%016" PRIx64 "/sofar/bm_rbr_data", getNodeId());
  configASSERT(topiclen > 0);
  return topiclen;
}
