#include "user_code.h"
#include "FreeRTOS.h"
#include "app_util.h"
#include "bm_rbr_data_msg.h"
#include "bsp.h"
#include "debug.h"
#include "device_info.h"
#include "pubsub.h"
#include "rbr_sensor.h"
#include "sensorWatchdog.h"
#include "spotter.h"
#include "uptime.h"

static constexpr char BM_RBR_WATCHDOG_ID[] = "bm_rbr";
static constexpr uint32_t PAYLOAD_WATCHDOG_TIMEOUT_MS = 10 * 1000;
// Note that PROBE_TIME_PERIOD_MS should be different than the watchdog timeout
// to avoid always trying to probe while the sensor is powered down.
static constexpr uint32_t PROBE_TIME_PERIOD_MS = 8 * 1000;
static constexpr uint32_t BM_RBR_DATA_MSG_MAX_SIZE = 256;
static constexpr uint8_t NO_MAX_TRIGGER = 0;

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

void setup(void) {
  uint32_t sensor_type = static_cast<uint32_t>(BmRbrDataMsg::SensorType_t::UNKNOWN);
  get_config_uint(BM_CFG_PARTITION_SYSTEM, RbrSensor::CFG_RBR_TYPE,
                  strlen(RbrSensor::CFG_RBR_TYPE), &sensor_type);
  get_config_uint(BM_CFG_PARTITION_SYSTEM, CFG_FTL_RECOVERY_MS, strlen(CFG_FTL_RECOVERY_MS),
                  &ftl_recovery_ms);
  rbr_sensor.init(static_cast<BmRbrDataMsg::SensorType_t>(sensor_type), PROBE_TIME_PERIOD_MS);
  SensorWatchdog::SensorWatchdogAdd(BM_RBR_WATCHDOG_ID, PAYLOAD_WATCHDOG_TIMEOUT_MS,
                                    BmRbrWatchdogHandler, NO_MAX_TRIGGER,
                                    RbrSensor::RBR_RAW_LOG);
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
  if (rbr_sensor.getData(d)) {
    SensorWatchdog::SensorWatchdogPet(BM_RBR_WATCHDOG_ID);
    static uint8_t cbor_buf[BM_RBR_DATA_MSG_MAX_SIZE];
    memset(cbor_buf, 0, sizeof(cbor_buf));
    size_t encoded_len = 0;
    if (BmRbrDataMsg::encode(d, cbor_buf, sizeof(cbor_buf), &encoded_len) == CborNoError) {
      bm_pub_wl(bmRbrTopic, bmRbrTopicStrLen, cbor_buf, encoded_len, 0,
                BM_COMMON_PUB_SUB_VERSION);
    } else {
      printf("Failed to encode data message\n");
    }
  }

  // Probe sensor type periodically, write only, no read
  rbr_sensor.maybeProbeType(last_payload_power_on_time);
}

static bool BmRbrWatchdogHandler(void *arg) {
  (void)arg;
  spotter_log(0, RbrSensor::RBR_RAW_LOG, USE_TIMESTAMP, "DEBUG - attempting FTL recovery\n");
  spotter_log_console(0, "DEBUG - attempting FTL recovery");
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
