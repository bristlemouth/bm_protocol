#include "user_code.h"
#include "FreeRTOS.h"
#include "bm_printf.h"
#include "bm_pubsub.h"
#include "bm_rbr_data_msg.h"
#include "bsp.h"
#include "debug.h"
#include "device_info.h"
#include "rbr_sensor.h"
#include "sensorWatchdog.h"
#include "uptime.h"
#include "util.h"

static constexpr char BM_RBR_WATCHDOG_ID[] = "bm_rbr";
static constexpr uint32_t PAYLOAD_WATCHDOG_TIMEOUT_MS = 10 * 1000;
static constexpr uint32_t PROBE_TIME_PERIOD_MS = 8 * 1000;
static constexpr uint32_t BM_RBR_DATA_MSG_MAX_SIZE = 256;
static constexpr uint8_t NO_MAX_TRIGGER = 0;
static constexpr char DEPTH_CAL_CONFIG_STR[] = "rbrCodaDepthCalM";

extern cfg::Configuration *systemConfigurationPartition;
static RbrSensor rbr_sensor;
static char bmRbrTopic[BM_TOPIC_MAX_LEN];
static int bmRbrTopicStrLen;
static float depthConfigM = 0.0;

static bool BmRbrWatchdogHandler(void *arg);
static int createBmRbrDataTopic(void);

void setup(void) {
  /* USER ONE-TIME SETUP CODE GOES HERE */
  configASSERT(systemConfigurationPartition);
  uint32_t sensor_type = static_cast<uint32_t>(BmRbrDataMsg::SensorType_t::UNKNOWN);
  systemConfigurationPartition->getConfig("rbrCodaType", strlen("rbrCodaType"), sensor_type);
  rbr_sensor.init(static_cast<BmRbrDataMsg::SensorType_t>(sensor_type));
  SensorWatchdog::SensorWatchdogAdd(BM_RBR_WATCHDOG_ID, PAYLOAD_WATCHDOG_TIMEOUT_MS,
                                    BmRbrWatchdogHandler, NO_MAX_TRIGGER,
                                    RbrSensor::RBR_RAW_LOG);
  bmRbrTopicStrLen = createBmRbrDataTopic();
  IOWrite(&BB_VBUS_EN, 0);
  vTaskDelay(pdMS_TO_TICKS(100)); // Wait for Vbus to stabilize
  IOWrite(&BB_PL_BUCK_EN, 0);
}

void loop(void) {
  /* USER LOOP CODE GOES HERE */
  static BmRbrDataMsg::Data d;
  static uint32_t last_probe_time_ms = uptimeGetMs();
  if (uptimeGetMs() - last_probe_time_ms > PROBE_TIME_PERIOD_MS) {
    rbr_sensor.probeType();
    float readDepthConfigM;
    if (rbr_sensor.getDepthConfiguration(readDepthConfigM) &&
        readDepthConfigM != depthConfigM) {
      depthConfigM = readDepthConfigM;
      bm_fprintf(0, RbrSensor::RBR_RAW_LOG, "DEBUG - Depth configuration updated to %f\n",
                 depthConfigM);
      bm_printf(0, "DEBUG - Depth configuration updated to %f\n", depthConfigM);
      printf("DEBUG - Depth configuration updated to %f\n", depthConfigM);
      systemConfigurationPartition->setConfig(DEPTH_CAL_CONFIG_STR,
                                              strlen(DEPTH_CAL_CONFIG_STR), depthConfigM);
    }
    last_probe_time_ms = uptimeGetMs();
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
}

static bool BmRbrWatchdogHandler(void *arg) {
  (void)arg;
  bm_fprintf(0, RbrSensor::RBR_RAW_LOG, "DEBUG - attempting FTL recovery\n");
  bm_printf(0, "DEBUG - attempting FTL recovery\n");
  printf("DEBUG - attempting FTL recovery\n");
  IOWrite(&BB_PL_BUCK_EN, 1);
  vTaskDelay(pdMS_TO_TICKS(500)); // Wait for Vbus to stabilize
  rbr_sensor.flush();
  IOWrite(&BB_PL_BUCK_EN, 0);
  return true;
}

static int createBmRbrDataTopic(void) {
  int topiclen = snprintf(bmRbrTopic, BM_TOPIC_MAX_LEN, "sensor/%" PRIx64 "/sofar/bm_rbr_data",
                          getNodeId());
  configASSERT(topiclen > 0);
  return topiclen;
}
